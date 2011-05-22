/* module class */
#pragma once

#include <string>
#include <sstream>
#include <vector>
#include "err.h"

class Compiler;
class Program;
class SymbolTable;
class ReferenceTable;
class ByteChunk;
class Label;
class RomAccess;

class Module : public ErrorReceiver
{
private:
	std::string filename;
	std::string modulename;
	Compiler* parent;
	Program* program;
	SymbolTable* roottable;
	SymbolTable* importtable;
	ByteChunk* code;

	unsigned int baseaddress;

	bool failed;

	// Base number for unique internal labels
	unsigned int labelbase;

public:
	Module(const std::string& filename, Compiler* owner);
	Module(const std::string& filename, Compiler* owner, SymbolTable* root);

	std::string GetName() const;			// Returns the name of the module
	std::string GetFileName() const;		// Returns the filename of the module

	bool Failed() const;					// Returns true if compilation or evaluation of the module failed

	// DEPRECATED
	void SetLibTable(SymbolTable* lib);		// Assigns a parent to the root table for standard library symbols.
											// Yes, this makes the root table not really the "root" table -_-;
	void AddImport(const std::string&);		// 
	void Include(Module* other);			// Includes symbols from other module into this module's scope.
	std::vector<std::string>
		GetImports();						// Returns a vector of imports used by this module

	void Execute();							// Evaluates the module, collecting output in module's bytechunk
	void PrintAST() const;					// Prints the abstract syntax tree of the parsed code
	void PrintJumps() const;				// Prints labels defined in this module, with their addresses
	void PrintRootTable() const;			// Prints the root table of the module
	void PrintCode() const;					// Prints the binary code of the module

	SymbolTable* GetRootTable() const;		// Returns the root table of the module


	Module* GetSiblingContext				// Returns a sibling module
			(const std::string& name) const;


	// Label resolution methods
	//void RegisterLabel(Label* lbl);			// Registers a label in this module
	void SetBaseAddress(unsigned int addr);	// Sets the base address of the module for reference resolution
	unsigned int GetBaseAddress() const;	// Returns the base address of the module
	void ResolveReferences();				// Resolves all registered references

	// Returns a label name that is unique within this context, for use by internal lowering operations
	std::string GetUniqueLabelName() {
		std::stringstream ss;
		ss << labelbase++;
		return ss.str();
	}


	// Code manipulation stuff
	ByteChunk* GetCodeChunk() const { return code; }
	unsigned int GetCodeSize() const;
	void WriteCode(char* buffer, int location, int bufsize) const;

	// Registers a statement that will write some expression to an arbitrary
	// location within the output file after everything has been linked
	void RegisterRomWrite(RomAccess* w);

	// Implementation of ErrorReceiver
	void Error(const std::string&,int,int);
	void Warning(const std::string&,int,int);


	static bool CheckName(const std::string& name);
	static std::string NameFromFilename(const std::string& filename);

private:
	void Load(const std::string& filename);

	std::vector<std::string> GetImportsDefining(const std::string& id);
};

