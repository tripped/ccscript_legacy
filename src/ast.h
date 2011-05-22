/* AST class definitions */
#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include "err.h"

#include "value.h"	// Included here (instead of a forward decl) because
					// Value is a simple type meant to be passed and
					// returned mainly by value.
					// TODO: there are still plenty of places where we should
					//  replace includes with forward delcarations though :)

typedef enum {
	program,
	blockstmt,
	blockexpr,
	commandstmt,
	conststmt,
	labelstmt,
	exprstmt,
	ifexpr,
	menuexpr,
	intexpr,
	stringexpr,
	flagexpr,
	andexpr,
	orexpr,
	notexpr,
	identexpr,
	boundedexpr,
	romwritestmt,
	countexpr,		// more duct tape! yay!
	ambiguousid,
	errorexpr
} nodetype;


// Forward declarations
class SymbolTable;
class ByteChunk;
class Compiler;
class Module;
class Anchor;


/*
 * A context of evaluation for a node.
 * Consists of a reference to the module in which the node exists, as well
 * as some flags affecting how the node should be interpreted. The context
 * is separate from the node's scope, or the lexical environment in which
 * it will be evaluated.
 */
class EvalContext
{
public:
	Compiler* compiler;		// Compiler evaluating the module
	Module* module;			// Module in which the node is being evaluated
	SymbolTable* labels;	// The symbol table in which to define labels
	ByteChunk* output;		// Byte chunk where evaluation output will be written

	std::string file;		// Current file being executed
	int line;				// Current line being executed

	bool norefs;			// Do not register any references

	//bool isboolean;		// whether this node is being evaluated as part of a boolean expression
							// (REMOVED: actually, this really works best as a parameter with a
							//	default value - nodes shouldn't have to worry about clearing isboolean)
							// TODO: actually, this is probably the best place for this property after all.
							// nodes that change it can just restore the default value before returning.

	// HACK! Just for identifying labels in commands. :3
	std::string localscopename;

	std::string GetUniqueLabelName();

	void DefineAnchor(Anchor* lbl);
	void DefineAnchor(const std::string& name, Anchor* lbl);

	EvalContext() {
		module = NULL;
		labels = NULL;
		output = NULL;
		norefs = false;
	}
};


/*
 * The base class for all AST node classes
 */
class Node
{
public:
	Node(int line, ErrorReceiver* e) {
		this->linenumber = line;
		this->e = e;
	}
	virtual ~Node() {}
	virtual std::string ToString(const std::string& indent, bool suppress=false) const = 0;
	virtual nodetype GetType() const = 0;
	virtual bool IsExpression() const { return false; }

	void SetErrorHandler(ErrorReceiver* e) { this->e = e; }
	void Error(const std::string& msg) { if(e) e->Error(msg, linenumber, -1); }
	void Warning(const std::string& msg) { if(e) e->Warning(msg, linenumber, -1); }
	
	// Called to perform the initial typechecking pass before evaluation;
	// used to define global identifiers and signal errors on certain
	// constructs, such as labels used below global scope.
	virtual void PreTypecheck(SymbolTable* root, bool atroot) { };

private:
	// Disallow copy construction and assignment
	Node(const Node&);
	void operator=(const Node&);

protected:
	inline static std::string nest(const std::string& i) {
		return "   " + i;
	}

protected:
	int linenumber;
	ErrorReceiver* e;
};


/*
 * Base class for expression-type nodes
 */
class Expression : public Node
{
public:
	Expression(int line, ErrorReceiver* e) : Node(line, e) { scope = NULL; }
	SymbolTable* scope;	// the scope in which to evaluate the expression

	// Evaluates the expression and returns its value
	//  TODO -- remove 'asbool' parameter; default param values are bad mojo anyway
	virtual Value Evaluate(SymbolTable* env, EvalContext& context, bool asbool=false) = 0;

	bool IsExpression() const { return true; }
};


/*
 * Base class for statement nodes
 */
class Statement : public Node
{
public:
	Statement(int line, ErrorReceiver* e) : Node(line, e) { }

	// Executes the statement
	virtual void Do(SymbolTable* env, EvalContext& context) = 0;
};



/*
 * Statements
 */
class Block : public Statement
{
private:
	std::vector<Statement*> stmts;
	bool noscope;	// HACK

public:
	Block(int line, ErrorReceiver* e = NULL)
		: Statement(line, e), noscope(false) {
	}
	~Block() {
		while(!stmts.empty()) {
			delete stmts.back();
			stmts.pop_back();
		}
	}
	void Add(Statement* stmt) {
		stmts.push_back(stmt);
	}
	nodetype GetType() const { return blockstmt; }

	void NoLocalScope(bool n) {
		noscope = n;
	}

	// defined in ast.cpp
	void PreTypecheck(SymbolTable*, bool);
	void Do(SymbolTable*, EvalContext&);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};


/*
 * Block "expression"
 * Basically a grammatical wrapper for interpreting a block statement as an expression.
 */
class BlockExpr : public Expression
{
private:
	Block* block;
public:
	BlockExpr(int line, Block* b, ErrorReceiver* e = NULL)
		: Expression(line, e), block(b) { }
	~BlockExpr() {
		delete block;
	}
	void NoLocalScope(bool n) {
		block->NoLocalScope(n);
	}
	nodetype GetType() const { return blockexpr; }
	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable*, EvalContext&, bool asbool = false);
	std::string ToString(const std::string&, bool suppress = false) const;
};


/*
 * Another statement->expression fix!
 *
 * Label definitions must now be expressions also, so that they can return a value
 * which will be included in the result of any block expression containing a label.
 *
 * The value of a label expression is an empty string containing an anchor with
 * the appropriate name registered in the current scope.
 */

class Label : public Expression
{
private:
	std::string name;
	unsigned int address;	// the address this label points to; computed during codegen
	Anchor* anchor;			// generated anchor for this lexical label
public:
	Label(int line, const std::string& name, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->name = name;
		this->address = 0;
	}
	
	Label(const std::string& name) : Expression(-1, NULL) {
		this->name = name;
	}

	nodetype GetType() const { return labelstmt; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable*, bool);
	Value Evaluate(SymbolTable*, EvalContext&, bool asbool = false);
	//void Do(SymbolTable* scope, EvalContext& context);
	std::string ToString(const std::string& indent, bool suppress =false) const;
};


class IfExpr : public Expression
{
private:
	Expression* condition;
	Expression* thenexpr;
	Expression* elseexpr;

public:
	IfExpr(int line, Expression* exp, Expression* thenexpr, Expression* elseexpr, ErrorReceiver* e = NULL)
		: Expression(line, e)
	{
		this->condition = exp;
		this->thenexpr = thenexpr;
		this->elseexpr = elseexpr;
	}
	// Destructor implemented in ast.cpp
	~IfExpr();
	nodetype GetType() const { return ifexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable*, bool);
	Value Evaluate(SymbolTable*, EvalContext&, bool asbool=false);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};

class MenuExpr : public Expression
{
private:
	std::vector<Expression*> options;
	std::vector<Expression*> results;
	unsigned int columns;
	int defaultopt;
	bool defcolumns;

public:
	MenuExpr(int line, ErrorReceiver* e = NULL) : Expression(line, e) {
		columns = 0;
		defcolumns = true;
		defaultopt = -1;
	}
	// destructor implemented in ast.cpp
	~MenuExpr();

	void Add(Expression* option, Expression* result) {
		options.push_back(option);
		results.push_back(result);
		columns++;
	}
	void SetColumns(int n) { defcolumns = false; columns = n; }
	void SetDefault(int n) {
		if(defaultopt != -1)
			Warning("menu has more than one default option");
		defaultopt = n;
	}
	nodetype GetType() const { return menuexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable*, bool);
	Value Evaluate(SymbolTable*, EvalContext&, bool asbool=false);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};

class CommandDef : public Statement
{
private:
	std::string name;
	Expression* body;
	std::vector<std::string> args;	// names of arguments

	//SymbolTable* scope;		// local scope

	SymbolTable* parentScope;	// lexical parent scope

	bool executing;			// whether this command is currently being evaluated.
							// this is a simple control to prevent recursion.

public:
	CommandDef(int line, const std::string& name, ErrorReceiver* e = NULL) : Statement(line, e) {
		this->name = name;
		body = NULL;
		parentScope = NULL;
		executing = false;
	}
	// defined in ast.cpp
	~CommandDef();

	void AddArg(const std::string& name) {
		args.push_back(name);
	}
	void SetBody(Expression* body) {
		this->body = body;
	}
	size_t GetArgCount() const { return args.size(); }
	nodetype GetType() const { return commandstmt; }

	// Typechecking and evaluation methods, defined in ast.cpp
	void PreTypecheck(SymbolTable*, bool);
	void Do(SymbolTable* scope, EvalContext& context);
	Value Invoke(EvalContext& context, const std::vector<Expression*>& args);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};

//
// Const macro definition
//
class ConstDef : public Statement
{
private:
	std::string name;
	Expression* value;

	bool evaluating;

public:
	ConstDef(int line, const std::string& name, Expression* value, ErrorReceiver* e = NULL)
		: Statement(line, e)
	{
		this->name = name;
		this->value = value;
		this->evaluating = false;
	}
	~ConstDef() {
		delete value;
	}
	nodetype GetType() const { return conststmt; }
	Expression* GetValue() const { return value; }

	// Typechecking and evaluation methods defined in ast.cpp
	void PreTypecheck(SymbolTable* roottable, bool);
	void Do(SymbolTable* scope, EvalContext& context);

	//  TODO: this will probably be factored out in coming CCScript 2.0 updates
	Value EvaluateExpr(SymbolTable* scope, EvalContext& context, bool asbool=false);

	std::string ToString(const std::string& indent, bool suppress = false) const;
};


// An expression when it appears as a statement 
class ExprStmt : public Statement
{
private:
	Expression* expr;
public:
	ExprStmt(int line, Expression* expr, ErrorReceiver* e = NULL) : Statement(line, e) {
		this->expr = expr;
	}
	~ExprStmt() {
		delete expr;
	}
	nodetype GetType() const { return exprstmt; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	void Do(SymbolTable* scope, EvalContext& context);
	std::string ToString(const std::string& indent, bool suppress = false) const;
};


// This is a funky one. A RomWrite is a statement that causes data to be written
// directly into the ROM at a more or less arbitrary point. A RomWrite evaluates
// its value expression and address expression into ByteChunk caches, which are
// then used in the final pass of the compiler to determine where and what to write
// into the output file.

//
// BUGFIX: 12/11/2008
// Apparently I forgot about ROM access statements while I was refining the
// parse tree interpretation aspect of the compiler. :P
// Long story short, command definitions and invocations allow one node in the
// program tree (the command's body) to be evaluated multiple times; thus, nodes
// should not contain evaluation state that will be needed later, since it might
// be overwritten by a repeat evaluation.
//
// This is very much the case with ROM write statements: previously each RomWrite
// node was storing its component expressions and evaluation caches for each,
// and registering a pointer to itself (!) with the compiler on evaluation. The upshot
// of this being that a ROM[] statement in a command definition (say, sprite_link)
// will behave erroneously; only the last evaluation of the node will have any effect.
//
// To fix this, we eliminate the statefulness of the RomWrite class, and introduce
// a new class, an instance of which will be created at each evaluation of a RomWrite,
// and registered with the compiler.
//
// TODO:
//  Since this isn't an AST class, it should probable be moved to its own file.
//  We should also do this with other entities that are generated by statements,
//  like commands and labels.

// New class //
class RomAccess
{
public:
	// Code and context caching
	SymbolTable* internal_labels;
	ByteChunk* cache_base;
	ByteChunk* cache_size;
	ByteChunk* cache_index;
	ByteChunk* cache_value;

public:
	RomAccess()
	{
		internal_labels = NULL;
		cache_base = NULL;
		cache_size = NULL;
		cache_index = NULL;
		cache_value = NULL;
	}
	void ResolveReferences();
	unsigned int GetVirtualAddress() const;
	void DoWrite(char* buffer, unsigned int address, int bufsize);
};

// TODO: rename this class to "RomAccessStmt" for consistency.
class RomWrite : public Statement
{
private:
	Expression* base;
	Expression* size;
	Expression* index;
	Expression* value;

public:
	RomWrite(int line, ErrorReceiver* e = NULL) : Statement(line, e) {
		base = NULL;
		size = NULL;
		index = NULL;
		value = NULL;
	}
	~RomWrite() {
		delete base;
		delete size;
		delete index;
		delete value;
	}
	void SetBase(Expression* base) { this->base = base; }
	void SetSize(Expression* size) { this->size = size; }
	void SetIndex(Expression* index) { this->index = index; }
	void SetValue(Expression* value) { this->value = value; }
	nodetype GetType() const { return romwritestmt; }
	
	// defined in ast.cpp
	void Do(SymbolTable* scope, EvalContext& context);
	std::string ToString(const std::string& indent, bool s = false) const;
};


/***************
 * Expressions *
 ***************/


class IntLiteral : public Expression
{
private:
	int value;
public:
	IntLiteral(int line, int value, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->value = value;
	}
	nodetype GetType() const { return intexpr; }

	// defined in ast.cpp
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};


class StringLiteral : public Expression
{
private:
	std::string value;
public:
	StringLiteral(int line, const std::string& value, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->value = value;
	}
	nodetype GetType() const { return stringexpr; }

	// defined in ast.cpp
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};


/*
 * An integer that should be interpreted as an event flag
 */
class FlagExpr : public Expression
{
private:
	//int flag;
	Expression* expr;
public:
	FlagExpr(int line, Expression* expr, ErrorReceiver* e = NULL)
		: Expression(line, e), expr(expr)
	{ }
	nodetype GetType() const { return flagexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool suppress=false) const;
};



class AndExpr : public Expression
{
private:
	Expression* a;
	Expression* b;

public:
	AndExpr(int line, Expression* a, Expression* b, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->a = a;
		this->b = b;
	}
	~AndExpr() {
		delete a; delete b;
	}
	nodetype GetType() const { return andexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool s=false) const;
};



class OrExpr : public Expression
{
private:
	Expression* a;
	Expression* b;

public:
	OrExpr(int line, Expression* a, Expression* b, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->a = a;
		this->b = b;
	}
	~OrExpr() {
		delete a; delete b;
	}
	nodetype GetType() const { return orexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool s=false) const;
};


class NotExpr : public Expression
{
private:
	Expression *a;
public:
	NotExpr(int line, Expression* a, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->a = a;
	}
	~NotExpr() {
		delete a;
	}
	nodetype GetType() const { return notexpr; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool s=false) const;
};


/*
 * Represents a usage of an identifier symbol, with or without arguments
 */
class IdentExpr : public Expression
{
private:
	std::string file;
	std::string name;
	std::vector<Expression*> args;
	bool hasparens;	// true if parens '()' were used, even if no arguments
public:
	IdentExpr(int line, const std::string& file, const std::string& name,
			ErrorReceiver* e = NULL) : Expression(line, e) {
		this->file = file;
		this->name = name;
		this->hasparens = false;
	}
	~IdentExpr() {
		while(!args.empty()) {
			delete args.back(); args.pop_back();
		}
	}
	void UseParens() {
		hasparens = true;
	}
	void AddArg(Expression* arg) {
		args.push_back(arg);
	}
	nodetype GetType() const { return identexpr; }

	std::string GetFullName() const {
		if(!file.empty()) return file + "." + name;
		return name;
	}

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	std::string ToString(const std::string& indent, bool s=false) const;
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
};


/*
 * Restricts evaluation of an expression to a certain size or byte pattern
 * (for example, constrain size to a two-byte integer)
 */
class BoundedExpr : public Expression
{
private:
	Expression* expr;
	int size;
	int index;
public:
	BoundedExpr(int line, int max, ErrorReceiver *e) : Expression(line, e) {
		this->size = max;
		this->index = -1;
		this->expr = NULL;
	}
	void SetSize(int n) { size = n; }
	void SetIndex(int n) { index = n; }
	void SetExpr(Expression* e) { expr = e; }
	~BoundedExpr() {
		delete expr;
	}
	nodetype GetType() const { return boundedexpr; }

	// implemented in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	std::string ToString(const std::string& indent, bool s=false) const;
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
};


class CountExpr : public Expression
{
private:
	std::string id;
	int offset;
	int multiple;

	bool set;
	int value;

	// It's alright to cache the result here because each lexical
	// CountExpr implicitly has a specific value.
	Value cached_value;

public:
	CountExpr(int line, const std::string& id, int offset, int multiple, ErrorReceiver* e)
		: Expression(line, e), id(id), offset(offset),
          multiple(multiple), set(false), value()
	{ }
	// This overload is horrible.
	// Oh well, so is this whole class; it's getting abandoned with
	// the transition towards Banana.
	CountExpr(int line, const std::string& id, int value, ErrorReceiver* e)
		: Expression(line, e), id(id), set(true), value(value)
	{ }
	nodetype GetType() const { return countexpr; }

	void PreTypecheck(SymbolTable* root, bool atroot);
	Value Evaluate(SymbolTable* scope, EvalContext& context, bool asbool=false);
	std::string ToString(const std::string& indent, bool s=false) const;

private:
	static std::map<std::string,int> counters;

	static int GetCounter(const std::string& id) {
		std::map<std::string,int>::const_iterator it = counters.find(id);
		if(it == counters.end())
			return 0;
		return it->second;
	}
	static void SetCounter(const std::string& id, int val) {
		counters[id] = val;
	}
};


/*
 * A debugging expression, inserted in the AST wherever we find a HORRIBLE ERROR
 */
class ErrorExpr : public Expression
{
private:
	std::string msg;
public:
	ErrorExpr(int line, const std::string& msg, ErrorReceiver* e = NULL) : Expression(line, e) {
		this->msg = msg;
	}
	std::string ToString(const std::string& indent, bool s=false) const {
		return "/* THERE WAS AN ERROR HERE: " + msg + " */";
	}
	nodetype GetType() const { return errorexpr; }
	Value Evaluate(SymbolTable*, EvalContext&, bool asbool=false) { return Value(); }
};


class AmbiguousID : public Node
{
private:
	std::string id;
	std::vector<std::string> modules;
public:
	AmbiguousID(const std::string& id, ErrorReceiver* e) : Node(0, e), id(id)
	{ }
	nodetype GetType() const { return ambiguousid; }
	void AddModule(const std::string& m) {
		modules.push_back(m);
	}
	std::string ToString(const std::string& indent, bool s=false) const {
		std::string r = "identifier '" + id + "' is ambiguous; could be ";
		for(std::vector<std::string>::const_iterator it = modules.begin();
			it != modules.end(); ++it)
		{
			if(it+1 == modules.end())
				r += "or " + *it + "." + id;
			else
				r += *it + "." + id + ", ";
		}
		return r;
	}
};


/* The root class for our AST */
// TODO:
//  Maybe this should be derived from Statement, or more likely Block.
//  It's almost completely equivalent to Block.
class Program : public Node
{
private:
	std::vector<Statement*> stmts;
public:
	std::vector<std::string> imports;

	Program(int line, ErrorReceiver* e = NULL) : Node(line, e) { }
	~Program() {
		while(!stmts.empty()) {
			delete stmts.back(); stmts.pop_back();
		}
	}
	void Add(Statement* stmt) {
		stmts.push_back(stmt);
	}
	nodetype GetType() const { return program; }

	// defined in ast.cpp
	void PreTypecheck(SymbolTable* root, bool atroot);
	void Run(SymbolTable* scope, EvalContext& context);
	std::string ToString(const std::string& indent = "", bool s = false) const;
};
