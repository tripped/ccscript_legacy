/* Definition of typechecking and evaluation functions for AST */

#include "ast.h"

#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>

#include "anchor.h"
#include "symboltable.h"
#include "string.h"
#include "module.h"
#include "stringparser.h"
#include "exception.h"
#include "compiler.h"

using namespace std;

/*
 * Context methods
 */

void EvalContext::DefineAnchor(Anchor* label)
{
	if(!labels)
		throw Exception("context missing labels table");
	labels->DefineAnchor(label);
}

void EvalContext::DefineAnchor(const string& name, Anchor* label)
{
	if(!labels)
		throw Exception("context missing labels table");
	labels->DefineAnchor(name, label);
}

string EvalContext::GetUniqueLabelName()
{
	if(!module)
		throw Exception("context missing module");
	return module->GetUniqueLabelName();
}

/*
 * AST node constructors/destructors
 */

CommandDef::~CommandDef()
{
	delete body;
	//delete scope;
}

IfExpr::~IfExpr() {
	delete condition;
	delete thenexpr;
	delete elseexpr;
}

MenuExpr::~MenuExpr() {
	while(!options.empty()) {
		delete options.back();
		options.pop_back();
	}
	while(!results.empty()) {
		delete results.back();
		results.pop_back();
	}
}


/*
 * Pre-pass typechecking and root table construction
 */

void Block::PreTypecheck(SymbolTable* root, bool atroot)
{
	// Blocks have their own lexical scopes, which we don't
	// perform PreTypecheck on until just before evaluation.
}

void BlockExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	block->PreTypecheck(root, false);
}

void IfExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	condition->PreTypecheck(root, false);
	thenexpr->PreTypecheck(root, false);
	if(elseexpr)
		elseexpr->PreTypecheck(root, false);
}

void MenuExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	vector<Expression*>::const_iterator it;
	for(it = options.begin(); it != options.end(); ++it)
		(*it)->PreTypecheck(root, false);
	for(it = results.begin(); it != results.end(); ++it)
		(*it)->PreTypecheck(root, false);
}

void ConstDef::PreTypecheck(SymbolTable* root, bool atroot)
{
	if(!atroot) {
		Error("constants can only be defined at global scope");
		return;
	}
	if(root->Lookup(this->name) != Value::Undefined) {
		string err = "repeat definition of identifier '" + name + "'";
		Error(err);
		return;
	}
	root->Define(this->name, Value(this));

	value->PreTypecheck(root, false);
}


void CommandDef::PreTypecheck(SymbolTable* root, bool atroot)
{
	if(!atroot) {
		Error("commands can only be defined at global scope");
		return;
	}
	if(root->Lookup(this->name) != Value::Undefined) {
		string err = "repeat definition of identifier '" + name + "'";
		Error(err);
		return;
	}
	
	root->Define(this->name, Value(this));

	this->parentScope = root;

	// Create a temporary scope, just to check for repeat parameter definitions
	SymbolTable* scope = new SymbolTable(root);
	for(unsigned int i = 0; i < args.size(); ++i) {
		if(scope->Define(args[i], Value::Null)) {
			string err = "repeat definition of parameter '" + args[i] + "'";
			Error(err);
		}
	}
	delete scope;
}

void Label::PreTypecheck(SymbolTable* scope, bool atroot)
{
	if(scope->Get(this->name) != Value::Undefined ||
		scope->GetAnchor(this->name) != NULL) {
		string err = "repeat definition of identifier '" + name + "'";
		Error(err);
		return;
	}

	Anchor* a = new Anchor(name);
	a->SetExternal(true);
	scope->DefineAnchor(a);
}

void ExprStmt::PreTypecheck(SymbolTable* root, bool atroot)
{
	expr->PreTypecheck(root, atroot);
}

void AndExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	a->PreTypecheck(root, atroot);
	b->PreTypecheck(root, atroot);
}

void OrExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	a->PreTypecheck(root, atroot);
	b->PreTypecheck(root, atroot);
}

void NotExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	a->PreTypecheck(root, atroot);
}

void FlagExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	expr->PreTypecheck(root, atroot);
}

void BoundedExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	expr->PreTypecheck(root, atroot);
}

void IdentExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	vector<Expression*>::const_iterator it;
	for(it = args.begin(); it != args.end(); ++it)
	{
		(*it)->PreTypecheck(root, atroot);
	}
}

void Program::PreTypecheck(SymbolTable *root, bool atroot)
{
	for(unsigned int i = 0; i < stmts.size(); ++i)
		stmts[i]->PreTypecheck(root, true);
}


/*
 * Code lowering
 */

void Block::Do(SymbolTable *env, EvalContext& context)
{
	// Create a new scope unless specifically overridden
	SymbolTable* scope;
	if(noscope)
		scope = env;
	else
		scope = new SymbolTable(env);

	for(unsigned int i = 0; i < stmts.size(); ++i)
		stmts[i]->PreTypecheck(scope, false);

	// Abort early if pretypecheck failed.
	// This is just to prevent certain duplicate/consequent error messages.
	if(context.module->Failed())
		return;

	for(unsigned int i = 0; i < stmts.size(); ++i)
		stmts[i]->Do(scope, context);

	if(!noscope)
		delete scope;
}

Value BlockExpr::Evaluate(SymbolTable *env, EvalContext& context, bool asbool)
{
	if(this->scope != NULL)
		env = this->scope;

	String* output = new String();

	// store old output and assign new
	String* old = context.output;
	context.output = output;

	block->Do(env, context);

	// restore the previous output
	context.output = old;

	// Return a value containing the collected output
	return Value(output);
}

Value IfExpr::Evaluate(SymbolTable *env, EvalContext& context, bool asbool)
{
	if(this->scope != NULL)
		env = this->scope;

	/*
	 * Lowering an if statement:
	 *
	 *  [condition]
	 *  [iffalse goto falselbl]
	 *  [thenstmt]
	 *  [goto endlbl]
	 * falselbl:
	 *  [elsestmt]
	 * endlbl:
	 */

	String* value = new String();
	
	// Create internal labels
	string labelbase = context.GetUniqueLabelName();
	Anchor* endanchor = new Anchor(labelbase + ".end");
	Anchor* falseanchor = new Anchor(labelbase + ".false");

	// First, we evaluate the condition
	Value cond_val = condition->Evaluate(env, context, true);

	// TODO: this might be an opportunity to do some typechecking on the returned value,
	// instead of just converting it to a string. Maybe some warnings would be appropriate?
	// (i.e., conditioning on a number value, which is almost always meaningless)

	// append cond_val to the output:
	value->Append(cond_val.ToCodeString());

	// Then, we output an "iffalse goto false" instruction, and register a jump reference
	value->Code("1B 02 FF FF FF FF");
	value->AddReference(value->GetSize() - 4, falseanchor);

	// Evaluate the "then" statement
	Value then_val = thenexpr->Evaluate(env, context);
	value->Append(then_val.ToCodeString());


	// Add a "goto end"
	// TODO: strictly speaking, we can dispense with this last goto when
	// there is no 'else' clause. We'll leave it here for now until we
	// get the first round of regression tests in place, and then we'll
	// update it along with the other evaluation refactoring.
	value->Code("0A FF FF FF FF");
	value->AddReference(value->GetPos() - 4, endanchor);

	// Set the position of the false anchor within the string
	value->AddAnchor(falseanchor);

	// Evaluate the "else" statement
	if(elseexpr) {
		Value else_val = elseexpr->Evaluate(env, context);
		value->Append(else_val.ToCodeString());
	}

	// Set the position of the "end" label
	value->AddAnchor(endanchor);

	return Value(value);
}


Value MenuExpr::Evaluate(SymbolTable* scope, EvalContext& context, bool asbool)
{
	if(this->scope != NULL)
		scope = this->scope;

	// Lowering a menu statement:
	// [19 02][option][02] - for each option
	// [1C 0C $cols][11][12]
	// [09 $num (statementjmps)]
	// [goto end]
	// [statement][goto end] - for each statement
	// label end:

	String* value = new String();

	// Create internal labels
	vector<Anchor*> anchors;
	string labelbase = context.GetUniqueLabelName();

	for(unsigned int i = 0; i < options.size(); ++i) {
		std::stringstream ss;
		ss << ".opt" << i;
		Anchor* a = new Anchor(labelbase + ss.str());
		anchors.push_back(a);
	}
	Anchor* endanchor = new Anchor(labelbase + ".end");

	// First, append the options between [19 02] and [02] codes
	for(unsigned int i = 0; i < options.size(); ++i) {
		value->Code("19 02");
		value->Append( options[i]->Evaluate(scope, context).ToCodeString() );
		value->Code("02");
	}

	// Next, append the option display commands
	// If we're only using two options, and no number of columns was specified,
	// use "1C 07", otherwise, use "1C 0C".
	if(options.size() == 2 && defcolumns)
		value->Code("1C 07");
	else
		value->Code("1C 0C");

	value->Byte(columns);// write exactly one byte for the column count
	value->Code("11 12");

	// Next, the multi-jump code
	value->Code("09");
	value->Byte(results.size());// write exactly one byte for the option count
	for(unsigned int i = 0; i < results.size(); ++i) {
		value->Code("FF FF FF FF");
		value->AddReference(value->GetPos() - 4, anchors[i]);
	}

	// Add a jump to the "default" option after the multi-jump, or end if no default
	value->Code("0A FF FF FF FF");
	if(defaultopt != -1)
		value->AddReference(value->GetPos() - 4, anchors[defaultopt]);
	else
		value->AddReference(value->GetPos() - 4, endanchor);


	// Finally, write out all the options, with a "goto end" after each
	// At each point we set the position of the relevant label.
	for(unsigned int i = 0; i < results.size(); ++i)
	{
		value->AddAnchor(anchors[i]);
		value->Append( results[i]->Evaluate(scope, context).ToCodeString() );

		// Add a "goto end" after every statement, in case it falls through
		value->Code("0A FF FF FF FF");
		value->AddReference(value->GetPos() - 4, endanchor);
	}

	// Last step: set position of the "end" label
	value->AddAnchor(endanchor);

	return Value(value);
}


void CommandDef::Do(SymbolTable* scope, EvalContext& context)
{
	// Command definitions actually don't do anything on the evaluation/lowering pass.
	// Their symbols are defined in the initial BuildRootTable pass, and their subnodes
	// are not evaluated except when an IdentExpr that calls a command is evaluated.
	// At that point, the caller binds the parameters to the argument symbols in the
	// command's local scope and calls CommandDef::Invoke, which evaluates its subnodes
	// in that scope.
}

Value CommandDef::Invoke(EvalContext& context, const vector<Expression*>& args)
{
	if(executing) {
		// TODO: this recursion protection also prevents simple composition,
		// e.g., foo(foo("hi")). We should try to find a better way of detecting
		// recursion.

		// Basically, we should notice that while we _are_ evaluating the function
		// within itself, or rather, evaluating one of its argument expressions
		// requires making another call to the function, this will not lead to
		// infinite recursion.

		// It only looks that way because of lazy evaluation. Bottom line, when
		// evaluating a parameter ID, we should turn off the recursion check.

		Error("recursion detected in evaluation of command '" + this->name + "'");
		return Value();	// return invalid value
	}
	/* NOTE: args check responsibility moved to caller
		if(args.size() != this->args.size()) {
		Error("incorrect number of parameters to command '" + this->name + "'");
		return;
	}*/
	executing = true;

	SymbolTable* scope = new SymbolTable( this->parentScope );

	// First, bind the args to the local scope
	for(unsigned int i = 0; i < args.size(); ++i) {
		scope->Define(this->args[i], args[i]);
	}

	// First, build the command scope
	body->PreTypecheck(scope, false);

	string oldname = context.localscopename;
	context.localscopename = name;

	// Then evaluate the body of the command in the local scope
	Value result = body->Evaluate(scope, context);

	context.localscopename = oldname;

	delete scope;
	executing = false;

	return result;
}


void ConstDef::Do(SymbolTable* scope, EvalContext& context)
{

}

Value ConstDef::EvaluateExpr(SymbolTable* scope, EvalContext& context, bool asbool)
{
	if(evaluating) {
		Error("recursion detected in evaluation of constant '" + this->name + "'");
		return Value();
	}
	evaluating = true;
	Value result = value->Evaluate(scope, context, asbool);
	evaluating = false;
	return result;
}


void RomWrite::Do(SymbolTable* scope, EvalContext& original_context)
{
	// Create a new context for the subexpressions
	EvalContext context;
	context.module = original_context.module;

	RomAccess* access = new RomAccess();

	// We keep track of our own internal labels here, instead of letting
	// the module manage them. The module simply adds its base address to
	// all label targets, while our labels ultimately should be measured
	// relative to the final write location of this RomWrite statement.
	access->internal_labels = new SymbolTable();
	context.labels = access->internal_labels;


	// Next we evaluate all applicable subexpressions, caching the results
	access->cache_base = new String( base->Evaluate(scope, context).ToCodeString() );
	if(size)
		access->cache_size = new String( size->Evaluate(scope, context).ToCodeString() );
	if(index)
		access->cache_index = new String( index->Evaluate(scope, context).ToCodeString() );
	access->cache_value = new String( value->Evaluate(scope, context).ToCodeString() );


	// TODO: registering a delayed write is really an operation of the compiler class,
	// not of any one module being compiled. Perhaps a reference to the compiler should
	// also be part of the context?
	context.module->RegisterRomWrite(access);
}

/*
 * Returns the virtual address 
 */
unsigned int RomAccess::GetVirtualAddress() const
{
	unsigned int base_adr = 0;
	unsigned int entry_size = 0;
	unsigned int entry_index = 0;

	base_adr = cache_base->ReadLong(0);

	if(cache_size)
		entry_size = cache_size->ReadLong(0);
	if(cache_index)
		entry_index = cache_index->ReadLong(0);

	return base_adr + entry_size * entry_index;
}

/*
 * Resolves any references contained in the code generated for this RomWrite
 */
void RomAccess::ResolveReferences()
{
	cache_base->ResolveReferences();
	if(cache_size) cache_size->ResolveReferences();
	if(cache_index) cache_index->ResolveReferences();

	// Before resolving refs in the value code, update internal label targets
	internal_labels->AddBaseAddress(GetVirtualAddress());
	cache_value->ResolveReferences();
}

void RomAccess::DoWrite(char* buffer, unsigned int address, int bufsize)
{
	cache_value->WriteChunk(buffer, address, bufsize);
}


Value IdentExpr::Evaluate(SymbolTable* scope, EvalContext& context, bool asbool)
{
	/// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;

	//context.file = this->file;
	//context.line = this->linenumber;

	// To evaluate an identifier expression:
	// First, we have to look up the symbol and check the type of its value.
	// If it's a constant, we just evaluate the constant (provided there are no
	// parameters given; if there are parameters, we should report an error.)
	// If it's a command, we need to:
	//  - bind each argument expression to the corresponding symbol in the commands
	//     parameter list.
	//  - invoke the command.

	Module* module = context.module;
	
	SymbolTable* lookupScope = scope;

	// If the ident expr's "file" field is not empty, we'll look it up in a different module
	if(!file.empty()) {
		Module* mod = module->GetSiblingContext(file);
		if(!mod) {
			Error("reference to nonexistent module '" + file + "'");
			return Value::Null;
		}
		lookupScope = mod->GetRootTable();
	}

	Value found = lookupScope->Lookup(name);

	if(found != Value::Undefined) {
		// In most cases, we just return the value.
		if(found.GetType() != Type::Macro)
		{
			// However, evaluated vars are not importable.
			if(lookupScope != scope) {
				Error("cannot access local variable declaration '" + name + "' in module '" + file + "'");
				return Value::Null;
			}
			return found;
		}

		Node* node = found.GetNode();
		Value result;

		if(node->GetType() == conststmt)
		{
			if(hasparens) {
				Error("'" + GetFullName() + "' refers to a constant; cannot use parentheses");
				return Value();
			}
			result = dynamic_cast<ConstDef*>(node)->EvaluateExpr(scope, context, asbool);
		}
		else if(node->GetType() == commandstmt)
		{
			for(unsigned int i = 0; i < args.size(); ++i)
				args[i]->scope = scope;

			CommandDef* cmd = dynamic_cast<CommandDef*>(found.GetNode());

			if(cmd->GetArgCount() != args.size())
				Error("incorrect number of parameters to command '" + GetFullName() + "'");
			else
				result = cmd->Invoke(context, args);
		}
		else if(node->GetType() == ambiguousid)
		{
			AmbiguousID* ambig = dynamic_cast<AmbiguousID*>(found.GetNode());
			Error(ambig->ToString(""));
			result = Value::Null;
		}
		else if(node->IsExpression())
		{
			result = dynamic_cast<Expression*>(node)->Evaluate(scope, context, asbool);
		}
		else
		{
			Error("invalid type");
		}
		return result;
	}

	// Didn't find it in the symbol table, check the jumps table
	Anchor* foundanchor = lookupScope->LookupAnchor(name);

	if(foundanchor) {
		if(hasparens) {
			Error("'" + GetFullName() + "' refers to a label; cannot use parentheses");
			return Value();
		}

		String* val = new String();
		val->Long(foundanchor->GetTarget());

		// The targeted label might not have its address computed yet, so we register a
		// reference to the target label (unless refs are forbidden by the context)
		if(!context.norefs)
			val->AddReference(val->GetPos()-4, foundanchor);

		return Value(val);
	}

	Error("use of undefined identifier '" + GetFullName() + "'");
	return Value();
}


Value Label::Evaluate(SymbolTable* scope, EvalContext &context, bool asbool)
{
	// The value of a label expression is an empty string
	// containing an anchor. The anchor was also registered
	// in the current scope in the PreTypecheck phase, so
	// other expressions can refer to this anchor.

	String* value = new String();

	Anchor* theAnchor = scope->LookupAnchor(name);

	if(!theAnchor) {
		Error("label evaluation lookup failed for '" + name + "' - probable internal compiler error!");
		return Value();
	}

	value->AddAnchor( theAnchor );

	return Value(value);
}

void ExprStmt::Do(SymbolTable *scope, EvalContext& context)
{
	// The expression statement is where the value of expressions are finally
	// written out to the current "ROM" context.
	Value val = expr->Evaluate(scope, context);
	context.output->Append(val.ToCodeString());
}

Value AndExpr::Evaluate(SymbolTable *scope, EvalContext& context, bool asbool)
{
	// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;

	// Lowering A and B:
	//  [A]
	//  [iffalse goto end]
	//  [B]
	//  label end:

	String* value = new String();

	// Create internal label
	string labelbase = context.GetUniqueLabelName();
	Anchor* endanchor = new Anchor(labelbase + ".end");

	// Evaluate the first operand
	value->Append( a->Evaluate(scope, context, true).ToCodeString() );

	// Add a jump to the end if the first operand is false
	value->Code("1B 02 FF FF FF FF");
	value->AddReference(value->GetPos()-4, endanchor);

	// TODO:
	//  Hm. I just realized that some boolean expressions (and and or) rely on reference
	//  resolution to operate correctly. Thus, it doesn't make sense to use them in ROM
	//  write statements at the moment, because ROM write statements occur after normal
	//  resolution, but without doing any resolution themselves. Perhaps ROM write statements
	//  should have a special resolution step to take care of stuff like this. (Perhaps
	//  the ROM data itself should be represented as a ByteChunk, with refs?)
	//  Anyway, don't worry about this for now, since using boolean expressions in a ROM
	//  write statement is not a very likely usage scenario.
	// UPDATE: 11/11/2008
	//  This issue has been fixed by moving the evaluation of ROM write subexpressions
	//  back to the evaluation pass of the compiler, and simply caching the results and
	//  resolving references in a later pass. However, it still might be worthwhile to
	//  consider alternative solutions; whole-ROM resolution seems interesting for example.

	// Evaluate the second operand
	value->Append( b->Evaluate(scope, context, true).ToCodeString() );

	// Set the position of the end label
	value->AddAnchor(endanchor);

	return Value(value);
}

Value OrExpr::Evaluate(SymbolTable *scope, EvalContext& context, bool asbool)
{
	// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;

	// Lowering of A or B:
	//  [A]
	//  [iftrue goto end]
	//  [B]
	//  label end:
	String* value = new String();

	string labelbase = context.GetUniqueLabelName();
	Anchor* endanchor = new Anchor(labelbase + ".end");

	// a
	value->Append( a->Evaluate(scope, context, true).ToCodeString() );
	// iftrue goto end:
	value->Code("1B 03 FF FF FF FF");
	value->AddReference(value->GetPos()-4, endanchor);
	// b
	value->Append( b->Evaluate(scope, context, true).ToCodeString() );
	// end:
	value->AddAnchor(endanchor);

	return Value(value);
}

Value NotExpr::Evaluate(SymbolTable *scope, EvalContext& context, bool asbool)
{
	// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;

	// Lowering of not A:
	// [A]			; assumes A modifies the W register
	// [0B 00]		; set W = (W == 0)
	String* value = new String();
	value->Append( a->Evaluate(scope, context, true).ToCodeString() );
	value->Code("0B 00");

	return Value(value);
}

Value FlagExpr::Evaluate(SymbolTable* scope, EvalContext& context, bool asbool)
{
	// When evaluating as a boolean, we want to use the "load flag" command, 07.
	// This is so an expression "flag <x>" can be used in normal expressions as
	// a flag number (e.g., set(myflag)) and in boolean conditions (e.g.,
	//  if myflag { }). Just sugar, really.
	// Thus, if statements and "and", "or", "not" expressions pass asbool=true.
	// All other nodes ignore this parameter, using the default value of false.
	//
	// Honestly, this feels like a hack, and it comes at the cost of some possibly
	// unintuitive behavior. However, it seems useful to allow the use of flags
	// in conditionals this way... Maybe there's a better alternative.

	String* value = new String();

	if(asbool) value->Code("07");

	Value flagval = expr->Evaluate(scope, context, false);

	String flagstr = flagval.ToCodeString();

	value->Append(flagstr.Substring(0,2));

	return Value(value);
}

Value IntLiteral::Evaluate(SymbolTable *scope, EvalContext& context, bool asbool)
{
	return Value(this->value);
}

Value StringLiteral::Evaluate(SymbolTable *scope, EvalContext& context, bool asbool)
{
	// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;

	// Use a stringparser to evaluate self
	StringParser parser(value, linenumber, e);
	return parser.Evaluate(scope, context);
}


Value BoundedExpr::Evaluate(SymbolTable* scope, EvalContext& context, bool asbool)
{
	/// Scope override; must do this for every expression that might contain an identifier
	if(this->scope != NULL)
		scope = this->scope;
	// TODO: there really has to be a better way to handle these scope overrides.
	//  Having to put this if statement at the top of certain evaluate methods is kludgy.
	//  Probably the logic for deciding the evaluation scope of a node should be at
	//  a higher level, and all nodes should have their scope of evaluation set that way.

	String* value = new String();

	Value expr_val = expr->Evaluate(scope, context);

	int pos;
	if(index < 0)
		pos = 0;
	else
		pos = size * index;

	try
	{
		// We've specified that any out-of-range access should be filled in
		// with zeroes, so we do a bit of bounds checking here
		String s = expr_val.ToCodeString();

		int over = std::max(0, pos + size - (signed)s.GetSize());
		int valid_size = std::max(0, size - over);

		if(valid_size > 0) {
			// We really ought to make a "substring constructor" --
			// we do this fairly often and it involves an unfortunate
			// amount of copying.
			*value = s.Substring(pos, valid_size);
		}
		for(int i = 0; i < size - valid_size; ++i)
			value->Byte(0);
	}
	catch(Exception& e)
	{
		Error(e.GetMessage());
	}

	return Value(value);
}


map<string,int> CountExpr::counters;

void CountExpr::PreTypecheck(SymbolTable* root, bool atroot)
{
	if(set) {
		SetCounter(id, this->value);
		cached_value = Value(new String());
	} else {
		int val = GetCounter(id);
		SetCounter(id, val+1);
		cached_value = Value(val * multiple + offset);
	}
}

Value CountExpr::Evaluate(SymbolTable *scope, EvalContext &context, bool asbool)
{
	return cached_value;
}


void Program::Run(SymbolTable* scope, EvalContext& context)
{
	for(unsigned int i = 0; i < stmts.size(); ++i)
		stmts[i]->Do(scope, context);
}



/*
 * AST "pretty-printing" methods.
 * Pretty much only used during debugging, when a nicely-formatted
 * representation of the parsed program is desired.
 */
string Block::ToString(const string& indent, bool suppress) const {
	string result = "{\n";
	for(unsigned int i = 0; i < stmts.size(); ++i)
		result += stmts[i]->ToString(nest(indent)) + "\n";
	result += indent + "}";
	return result;
}

string BlockExpr::ToString(const string& indent, bool suppress) const {
	return block->ToString(indent, suppress);
}

string IfExpr::ToString(const string& indent, bool suppress) const {
	string result = indent + "if ";
	result += condition->ToString(indent) + " ";
	result += thenexpr->ToString(indent, true) + " ";
	if(elseexpr != NULL) {
		result += "else ";
		result += elseexpr->ToString(indent, true);
	}
	return result;
}

string MenuExpr::ToString(const string& indent, bool suppress) const {
	string result = indent + "menu ";
	if(columns != options.size()) {
		stringstream ss;
		ss << columns;
		result += ss.str() + " ";
	}
	result += "{\n";
	for(unsigned int i = 0; i < options.size(); ++i) {
		result += nest(indent) + options[i]->ToString(indent) + ": " + results[i]->ToString(nest(indent),true) + "\n";
	}
	result += indent + "}";
	return result;
}

string CommandDef::ToString(const string& indent, bool suppress) const {
	string result = indent + "command " + name + " ";
	if(args.size() > 0) {
		result += "(" + args[0];
		for(unsigned int i = 1; i < args.size(); ++i)
			result += "," + args[i];
		result += ") ";
	}
	return result + body->ToString(indent);
}


string ConstDef::ToString(const string& indent, bool suppress) const {
	return indent + "define " + name + " = " + value->ToString(indent);
}

string RomWrite::ToString(const string& indent, bool suppress) const {
	string result = indent;
	if(size) {
		result += "ROMTBL[";
		result += base->ToString("",true) + ", ";
		result += size->ToString("",true) + ", ";
		result += index->ToString("",true) + "] = ";
	}
	else {
		result += "ROM[";
		result += base->ToString("",true) + "]";
	}
	result += value->ToString("",true);
	return result;
}

string Label::ToString(const string& indent, bool suppress) const {
	return indent + name + ": ";
}

string ExprStmt::ToString(const string& indent, bool suppress) const {
	if(suppress)
		return expr->ToString(indent);
	return indent + expr->ToString(indent);
}

string FlagExpr::ToString(const string&indent, bool suppress) const {
	return "flag " + expr->ToString(indent);
}

string IntLiteral::ToString(const string& indent, bool suppress) const {
	stringstream s;
	s << value;
	return s.str();
}

string StringLiteral::ToString(const string& indent, bool suppress) const {
	return "\"" + value + "\"";
}

string AndExpr::ToString(const string& indent, bool s) const {
	return "(" + a->ToString(indent) + " and " + b->ToString(indent) + ")";
}

string OrExpr::ToString(const string& indent, bool s) const {
	return "(" + a->ToString(indent) + " or " + b->ToString(indent) + ")";
}

string NotExpr::ToString(const string& indent, bool s) const {
	return "not " + a->ToString(indent);
}

string IdentExpr::ToString(const string& indent, bool s) const {
	string result;
	if(file != "")
		result += file + ".";
	result += name;
	if(args.size() > 0) {
		result += "(" + args[0]->ToString(indent,s);
		for(unsigned int i = 1; i < args.size(); ++i)
			result += ", " + args[i]->ToString(indent,s);
		result += ")";
	}
	else if(hasparens)
		result += "()";
	return  result;
}


string BoundedExpr::ToString(const string &indent, bool s) const
{
	stringstream ss;
	switch(size) {
		case 1: ss << "byte "; break;
		case 2: ss << "short "; break;
		case 4: ss << "long "; break;
		default:
			ss << "maxbytes " << size << " ";
	}
	if(index != -1) {
		ss << "[" << index << "] ";
	}
	ss << expr->ToString(indent, s);
	return ss.str();
}

string CountExpr::ToString(const string& indent, bool s) const
{
	stringstream ss;

	if(!set) {
		ss << "count(\"" << id << "\"";
		if(offset != 0 || multiple != 1) {
			ss << ", " << offset;
			ss << ", " << multiple;
		}
		ss << ")";
	} else {
		ss << "setcount(\"" << id << "\""
		   << ", " << value << ")";
	}

	return ss.str();
}


string Program::ToString(const string& indent, bool s) const {
	string result = "";
	for(unsigned int i = 0; i < stmts.size(); ++i)
		result += stmts[i]->ToString(indent) + "\n";
	return result;
}

