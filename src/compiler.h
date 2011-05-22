/* compiler class */
#pragma once

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>

class Module;
class SymbolTable;
class RomAccess;


class Compiler
{
public:
	bool printAST;
	bool printRT;
	bool printJumps;
	bool printCode;
	bool verbose;
	bool noreset;
	bool nostdlibs;
	std::string libdir;

public:
	Compiler(const std::string& romfile, unsigned int outadr, unsigned int endadr = 0);
	~Compiler();

	bool Failed() { return failed; }

	std::string FindModule(const std::string& name, const std::string& filedir);
	Module* FindAndLoadModule(const std::string& name, const std::string& filedir);
	Module* LoadModule(const std::string& filename);
	Module* GetModule(const std::string& name);

	// Errors
	void Error(const std::string& msg);
	void Warning(const std::string& msg);

	void Compile();
	void RegisterDelayedWrite(RomAccess* w);
	void DoDelayedWrites();
	void WriteOutput();
	void Results();

	void WriteSummary(std::ostream& out);




private:
	static unsigned int GetNextBank(unsigned int adr);

	unsigned int MapVirtualAddress(unsigned int adr);

	void ProcessImports();
	void EvaluateModules();
	void EvaluateLibraries();
	void AssignModuleAddresses();
	void OutputModules();

	void WriteResetInfo(const std::string& file);
	void ApplyResetInfo(const std::string& file);

	bool failed;	// indicates a fatal error in any module


private:
	int errorcount;
	int warningcount;

	std::vector<Module*> modules;
	std::vector<Module*> libs;
	SymbolTable* libtable;

	// File info
	std::string filename;
	char* filebuffer;
	int filesize;
	int actual_start;
	int actual_end;
	int totalfrag;

	// Header/headerless ROM
	bool has_header;

	// Output and addressing
	unsigned int outadr;
	unsigned int endadr;
	std::vector<RomAccess*> romwrites;
};

