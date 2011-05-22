/* definition for module class */

#include "module.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>

#include "compiler.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "symboltable.h"
#include "bytechunk.h"
#include "exception.h"

using namespace std;

void Module::Error(const string& msg, int line, int col)
{
	stringstream ss;
	ss << filename << ", line " << line << ": " << msg;
	parent->Error(ss.str());
	failed = true;
}

void Module::Warning(const string& msg, int line, int col)
{
	stringstream ss;
	ss << filename << ", line " << line << ": warning: " << msg;
	parent->Warning(ss.str());
}


/*
 * Constructs a code module from the given source file
 */
Module::Module(const string& filename, Compiler* parent)
{
	this->parent = parent;
	this->failed = false;
	this->roottable = new SymbolTable();
	Load(filename);
}

Module::Module(const string& filename, Compiler* parent, SymbolTable* root)
{
	this->parent = parent;
	this->failed = false;
	this->roottable = root;
	Load(filename);
}

/*
 * Loads and parses a module from a given source filename.
 * Once loaded, the module should be ready for evaluation if Failed() is false.
 */
void Module::Load(const string& filename)
{
	this->filename = filename;
	this->modulename = NameFromFilename(filename);
	// Check module name for validity
	if(!CheckName(modulename)) {
		parent->Error("module name '" + modulename + "' invalid. Module names can only contain alphanumeric characters and underscores.");
		failed = true;
		return;
	}
	ifstream in(filename.c_str());


	if(in.fail())
	{
		parent->Error("couldn't open " + filename);
		failed = true;
		return;
	}

	stringbuf sb;
	in >> std::noskipws >> &sb;
	program = NULL;

	// Parse the module
	Parser parser(sb.str());
	parser.SetErrorHandler(this);
	this->program = parser.Parse();
	if(failed) return;

	// After parsing, we know if the module includes any others


	// Build root table
	program->PreTypecheck(roottable, true);
	if(failed) return;

	importtable = new SymbolTable();
	roottable->SetParent(importtable);

	code = new ByteChunk();

	labelbase = 0;
}

/*
 * Produces a name string from a filename
 */
string Module::NameFromFilename(const string& filename)
{
	// Get just the part of the filename between the last directory separator and the first period
	size_t p1 = filename.find_last_of("/\\");
	if(p1 == string::npos)
		p1 = 0;
	else
		p1++;

	size_t p2 = filename.find('.', p1);
	if(p2 == string::npos)
		p2 = filename.length();

	return filename.substr(p1, p2-p1);
}

/*
 * Verifies that a string is a valid module name
 */
bool Module::CheckName(const string& name)
{
	// String must contain only alphanumeric characters and _
	// Also, cannot start with a number
	if(isdigit(name[0])) return false;
	for(unsigned int i = 0; i < name.length(); ++i) {
		if(!isalnum(name[i]) && name[i] != '_')
			return false;
	}
	return true;
}


/*
 * Returns the module's name.
 */
string Module::GetName() const
{
	return modulename;
}


/*
 * Returns the module's filename
 */
string Module::GetFileName() const
{
	return filename;
}


/*
 * Evaluates the program
 */
void Module::Execute()
{
	if(failed) {
		parent->Error("There were compilation errors. Cannot execute module.");
		return;
	}
	EvalContext context;
	context.module = this;
	context.compiler = this->parent;
	context.labels = this->GetRootTable();
	context.output = this->GetCodeChunk();
	program->Run(roottable, context);
}

/*
 * Returns true iff parsing or evaluation of the module failed.
 */
bool Module::Failed() const
{
	return failed;
}


SymbolTable* Module::GetRootTable() const
{
	return roottable;
}

/*
 * Assigns the "library" table, a "fallback" root scope
 * intended to contain standard library identifiers
 */
void Module::SetLibTable(SymbolTable *lib)
{
	roottable->SetParent(lib);
}

void Module::AddImport(const string& name)
{
	vector<string>::const_iterator it =
		find(program->imports.begin(), program->imports.end(), name);
	if(it == program->imports.end())
		program->imports.insert(program->imports.begin(), name);
}

void Module::Include(Module* other)
{
	// To do this, we create a chain of root tables.
	// Whenever an include is made, we insert a copy of the included
	// module's root table directly above this module's root table in
	// the chain. We then set the copied root table's parent to the
	// former parent of this module's root table.

	// Hmm. In this method, colliding symbols in includes would be
	// silently resolved to the last-included symbol. I don't think
	// this behavior is optimal. There should at least be a warning,
	// and quite possibly the symbol should be simply omitted from the
	// table, thus requiring resolution.

	// Okay, instead, here's what we'll do. Each module still has an
	// "inclusion" symbol table above its root table, and includes are
	// *merged* into that table. Collisions generate a warning and
	// result in the omission of all colliding symbols.

	// Merge symbols

	vector<string> collisions;
	importtable->Merge( *other->GetRootTable(), collisions );

	// Add ambiguity symbols for collisions
	for(vector<string>::const_iterator it = collisions.begin();
		it != collisions.end(); ++it)
	{
		Value existing = importtable->Get(*it);

		if(existing.GetType() != Type::Macro || existing.GetNode()->GetType() != ambiguousid) {
			AmbiguousID* ambig = new AmbiguousID(*it, this);

			// This is kind of a hack. Scan through all the modules
			// to find which ones include this symbol.
			vector<string> defining = GetImportsDefining(*it);

			for(vector<string>::const_iterator k = defining.begin();
				k != defining.end(); ++k)
			{
				ambig->AddModule(*k);
			}
			
			importtable->Define(*it, Value(ambig));
		}
		else {
			AmbiguousID* ambig = dynamic_cast<AmbiguousID*>(existing.GetNode());
			if(ambig)
				ambig->AddModule(other->GetName());
		}
	}
}

vector<string> Module::GetImports()
{
	return program->imports;
}

vector<string> Module::GetImportsDefining(const string& id)
{
	vector<string> result;
	for(vector<string>::const_iterator it = program->imports.begin();
		it != program->imports.end(); ++it)
	{
		Module* mod = parent->GetModule(*it);
		if(!mod) continue;

		if(mod->roottable->Get(id) != Value::Undefined ||
			mod->roottable->GetAnchor(id) != NULL)
			result.push_back(*it);
	}
	return result;
}





/*
 * Prints the parse tree of the program
 */
void Module::PrintAST() const
{
	if(!program) return;
	printf("Parse tree of %s\n", this->filename.c_str());
	printf("=============================================\n");
	printf("%s\n", program->ToString().c_str());
}

/*
 * Prints identifiers and commands defined at the global scope of the module
 */
void Module::PrintRootTable() const
{
	if(!roottable) return;
	printf("Root table -- %s\n", this->filename.c_str());
	printf("=============================================\n");
	printf("%s\n", roottable->ToString().c_str());
}

/*
 * Prints a table of defined labels, including addresses
 */
void Module::PrintJumps() const
{
	if(!roottable)	return;
	printf("Jump table -- '%s'\n", this->filename.c_str());
	printf("=============================================\n");
	printf("%s\n", roottable->JumpsTable().c_str());

	// TEMP: also print in-code anchors

}

/*
 * Prints compiled binary code
 */
void Module::PrintCode() const
{
	if(failed) return;
	printf("Compiled code -- '%s'\n", this->filename.c_str());
	printf("=============================================\n");
	code->PrintCode();
	printf("\n");
}

Module* Module::GetSiblingContext(const string& name) const
{
	return parent->GetModule(name);
}


/*
 * Registers a label in this module
 */
//void Module::RegisterLabel(Label* label)
//{
//	roottable->DefineLabel(label->GetName(), label);
//}


/*
 * Returns the compiled size of the module's code.
 * Only valid if the module has already been evaluated.
 */
unsigned int Module::GetCodeSize() const
{
	return code->GetSize();
}


/*
 * Sets the base virtual address of the module's code.
 */
void Module::SetBaseAddress(unsigned int addr)
{
	baseaddress = addr;
	roottable->AddBaseAddress(addr);
	// TODO: the jump table should probably be factored out of symboltable, I think.
	// (Or maybe not? Labels are symbols, after all, so it makes sense on that level
	//  to put them in a symbol table. The main concern is that currently, labels are
	//  always global symbols, so we always register them in the root symbol table,
	//  which exists at the module level.)

	// Update all in-code anchors
	code->SetBaseAddress(addr);
}

/*
 * Returns the base virtual address of the module's code.
 */
unsigned int Module::GetBaseAddress() const
{
	return baseaddress;
}

/*
 * Resolves references by replacing label target placeholders with the final
 * location of the label. Only valid after evaluation.
 */
void Module::ResolveReferences()
{
	code->ResolveReferences();
}

/*
 * Registers a delayed ROM write
 */
void Module::RegisterRomWrite(RomAccess *w)
{
	parent->RegisterDelayedWrite(w);
}

/*
 * Writes the modules code to the specified buffer.
 */
void Module::WriteCode(char* buffer, int location, int bufsize) const
{
	if(!code->WriteChunk(buffer, location, bufsize))
		throw Exception("attempt to write past end of ROM");
}

