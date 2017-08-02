/* compiler class implementation */

#include "compiler.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem::v1;

#include "module.h"

#include "anchor.h"
#include "ast.h"
#include "bytechunk.h"
#include "module.h"
#include "symboltable.h"
#include "exception.h"
#include "util.h"

using namespace std;

/*
 * Reports a compiler error
 */
void Compiler::Error(const string& msg)
{
	std::cerr << " error: " << msg << std::endl;
	errorcount++;
	failed = true;
}


/*
 * Reports a compiler warning
 */
void Compiler::Warning(const string& msg)
{
	std::cerr << " warning: " << msg << std::endl;
	warningcount++;
}


/*
 * Constructs a compiler object targeting the specified output file and address
 */
Compiler::Compiler(const string& romfile, unsigned int adr, unsigned int endadr)
{
	failed = false;
	errorcount = 0;
	warningcount = 0;
	filebuffer = NULL;
	libtable = NULL;
	filename = romfile;
	verbose = false;
	noreset = false;
	nostdlibs = false;

	// Open the file
	ifstream file(ConvertToNativeString(filename), ifstream::binary);

	if(file.fail()) {
		Error("failed to open file " + filename + " for reading.");
		return;
	}

	// Ensure that the address given is valid
	if(MapVirtualAddress(adr) == 0xFBADF00D) {
		stringstream ss;
		ss << "bad virtual address for start: " << std::setbase(16) << adr;
		Error(ss.str());
		return;
	}
	if((endadr != 0) && MapVirtualAddress(endadr) == 0xFBADF00D) {
		stringstream ss;
		ss << "bad virtual address for end: " << std::setbase(16) << endadr;
		Error(ss.str());
		return;
	}

	file.seekg(0, ifstream::end);
	filesize = file.tellg();
	file.seekg(0);
	filebuffer = new char[filesize];
	file.read(filebuffer, filesize);
	file.close();

	// Check for header
	has_header = ((filesize & 0x200) != 0);

	// Ensure ROM has integral number of banks
	if( filesize & 0xFDFF ) {
		stringstream ss;
		ss << filename << " has incorrect filesize: " << filesize << " bytes";
		Error(ss.str());
	}

	this->outadr = adr;
	this->endadr = endadr;
	libtable = new SymbolTable();
}

Compiler::~Compiler()
{
	while(!modules.empty()) {
		delete modules.back();
		modules.pop_back();
	}
// NOTE: libs no longer owns the pointers
//	while(!libs.empty()) {
//		delete libs.back();
//		libs.pop_back();
//	}
	while(!romwrites.empty())
	{
		delete romwrites.back();
		romwrites.pop_back();
	}
	delete libtable;
	delete[] filebuffer;
}


/*
 * Writes buffer to the output file
 */
void Compiler::WriteOutput()
{
	if(failed) return;
	ofstream file(ConvertToNativeString(filename), ofstream::binary);

	if(file.fail()) {
		Error("failed to open file " + filename + " for writing.");
		return;
	}
	file.seekp(0);
	file.write(filebuffer, filesize);
	file.close();
}

/*
 * Loads, parses, and performs initial typechecking of a code file
 */
Module* Compiler::LoadModule(const std::string &filename)
{
	//if(failed) return NULL;

	//if(verbose)
	//	std::cerr << "Compiling " << filename << "..." << std::endl;

	Module* m = new Module(filename, this);

	if(m->Failed()) {
		failed = true;
		return NULL;
	}

	if(printAST)
		m->PrintAST();

	// TODO:
	// instead of SetLibTable, we should use m->Include(othermodule).
	//m->SetLibTable(libtable);
	string name = m->GetName();
	
	if(GetModule(name) != NULL) {
		Error("attempt to redefine module " + name + "; module names must be unique");
		return NULL;
	}

	modules.push_back(m);
	return m;
}


/*
 * Searches for a module with a given name in the include path and
 * returns a relative path to it, if found.
 *
 * The directories checked are as follows:
 *
 *  0. If the path is complete, no searching is done
 *  1. The directory which contains the file performing the import.
 *  2. The project working directory
 *  3. The compiler's /lib directory.
 */

string Compiler::FindModule(const string& name, const string& filedir)
{
	// Complete paths aren't looked for in include directories
	if (fs::path(name).is_absolute())
		return fs::exists( name )? name : "";

	// First, try in the provided file directory.
	fs::path base(filedir);
	base /= name;
	if( fs::exists( base ) )
		return base.string();

	// Next, try in the compilation working directory
	if( fs::exists( name ) )
		return name;

	// Finally, check the libs directory.
	fs::path libpath = fs::path(libdir) / name;
	if( fs::exists(libpath) )
		return libpath.string();

	return "";
}

/*
 * Searches the include paths for a module and loads it if found.
 */
Module* Compiler::FindAndLoadModule(const string& name, const string& filedir)
{
	string found = FindModule(name, filedir);

	if(found.empty())
		return NULL;

	return LoadModule(found);
}

/*
 * Returns the module with the given name
 */
Module* Compiler::GetModule(const std::string &name)
{
	for(unsigned int i = 0; i < modules.size(); ++i) {
		string mname = modules[i]->GetName();
		if(modules[i]->GetName() == name)
			return modules[i];
	}
	return NULL;
}


/*
 * Evaluates all modules and produces output
 */
void Compiler::Compile()
{
	// Don't try to execute if a previous error stopped compilation
	if(failed)
		return;

	if(verbose) {
		// This is a pretty flagrant abuse of cerr. Oh well.
		std::cerr << "Compiling modules..." << std::endl;
	}

	string resetfile = filename + ".reset.txt";

	try
	{
		if(!noreset)
			ApplyResetInfo(resetfile);

		ProcessImports();
		EvaluateModules();
		AssignModuleAddresses();
		OutputModules();

		if(!failed && !noreset)
			WriteResetInfo(resetfile);

		DoDelayedWrites();
	}
	catch(Exception& e)
	{
		Error(e.GetMessage());
	}
}

void Compiler::ProcessImports()
{

	// TO BEGIN WITH, we have the set of modules that are explicitly
	// included in the project command line. We will extend this set
	// by also checking the modules imported by any of the modules
	// in the project. These in turn might import other modules, so
	// we need to traverse the whole dependency network.
	//
	// Initially we have a set of modules needing to be processed.
	//
	// Each module processed, we remove from the list.
	//
	// When processing a module, we check its list of imports. For
	// each imported module name, we search for a loaded module with
	// that name. If the module is already loaded, we continue.
	// If the module is not loaded, we search for it in the include
	// paths and, if it is found, load it and add it to the list of
	// modules that need processing.
	//

	vector<Module*> remaining = modules;

	while(!remaining.empty()) {
		Module* m = remaining.back();
		remaining.pop_back();

		if(!nostdlibs) {
			m->AddImport( (fs::path(libdir) / "std.ccs").string() );
			m->AddImport( (fs::path(libdir) / "stdarg.ccs").string() );
		}

		vector<string> imports = m->GetImports();

		for(vector<string>::const_iterator it = imports.begin();
				it != imports.end(); ++it)
		{
			Module *imp;
			string filename = *it;

			fs::path module_path( m->GetFileName() );
			fs::path module_dir = module_path.parent_path();

			string name = Module::NameFromFilename(filename);

			imp = GetModule(name);

			// If the imported module doesn't exist already, load it
			if(!imp) {
				imp = FindAndLoadModule( filename, module_dir.string() );

				// We'll need to process the newly loaded module's imports as well
				if( imp )
					remaining.push_back(imp);
			}
			else {
				// Check this new import; if it refers to the same file that we've already
				// included in the project, it's okay; otherwise it's an error, since module
				// names must be unique.
				fs::path newpath		( FindModule(filename, module_dir.string()) );
				fs::path existingpath	( imp->GetFileName() );

				if( !fs::equivalent(existingpath, newpath) ) {
					throw Exception("attempted to import " + newpath.string() + "; module name collides with " + existingpath.string());
				}
			}


			if( !imp )
				throw Exception("Couldn't find module '" + filename + "'");

			m->Include( imp );
		}
	}
}


/*
 * Evaluates all loaded modules
 */
void Compiler::EvaluateModules()
{
	// Evaluate each module to determine its code size
	for(unsigned int i = 0; i < modules.size(); ++i)
	{
		Module* m = modules[i];

		if(verbose && m->GetName().substr(0,3) != "std")	// This is a hack.
			std::cerr << "Evaluating " << m->GetFileName() << "..." << std::endl;
		m->Execute();

		if(m->Failed())
			failed = true;

		if(printRT && m->GetName().substr(0,3) != "std")
			m->PrintRootTable();

		// Any module larger than 64K is a fatal error.
		if(m->GetCodeSize() > 0x10000)
			throw Exception("module '" + m->GetName() + "' exceeds 64KB");
	}
}

/*
 * Predicate for comparing modules (used by AssignModuleAddresses)
 */
bool module_pred(Module* a, Module* b) {
	return a->GetCodeSize() > b->GetCodeSize();
}

/*
 * Assigns base addresses to all modules
 */
void Compiler::AssignModuleAddresses()
{
	if(failed)
		return;

	// Here, we set the base address of each module.
	// We do this by repeatedly selecting the largest module that will fit between
	// the current base and the next bank boundary, and assigning it that base address,
	// then increasing the base address by the size of the module. If we do not find a
	// module that will fit, we increase the base address to the start of the next bank.

	vector<Module*> sorted = modules;
	std::sort(sorted.begin(), sorted.end(), module_pred);
	unsigned int base = this->outadr;

	totalfrag = 0;
	actual_start = -1;
	actual_end = -1;

	while(!sorted.empty())
	{
		vector<Module*>::iterator i;
		bool found = false;
		for(i = sorted.begin(); i != sorted.end(); ++i)
		{
			unsigned int size = (*i)->GetCodeSize();		
			if((base & 0xFFFF) + size <= 0x10000) 
			{
				// Check for overwriting maximum address
				if((endadr > 0) && (base + size >= endadr))
					throw Exception("error: module " + (*i)->GetName() + " exceeded specified end address -- aborting");

				// Update write bounds
				if(actual_start == -1)
					actual_start = base;
				if(sorted.size() == 1)
					actual_end = base + size;

				(*i)->SetBaseAddress(base);
				base += size;
				sorted.erase(i);
				found = true;
				break;
			}
		}
		if(!found)
		{
			unsigned int nextbase = GetNextBank(base);
			if(!nextbase)
				throw Exception("fatal error - ran out of space writing module " + sorted[0]->GetName());

			totalfrag += (nextbase - base);
			base = nextbase;
		}
	}
}


/*
 * Resolves references and writes modules to the output file
 */
void Compiler::OutputModules()
{
	if(failed)
		return;

	if(verbose)
		std::cerr << "Writing output to ROM..." << std::endl;

	// Next, resolve all references in every module and write it to the buffer
	for(unsigned int i = 0; i < modules.size(); ++i) {
		Module* m = modules[i];
		m->ResolveReferences();

		unsigned int outadr = MapVirtualAddress(m->GetBaseAddress());

		if(outadr == 0xFBADF00D) {
			stringstream ss;
			ss << "Module has bad virtual address (" << std::setbase(16) << m->GetBaseAddress() << "), aborting";
			throw Exception(ss.str());
		}

		m->WriteCode(filebuffer, MapVirtualAddress(m->GetBaseAddress()), filesize);

		if(printJumps && m->GetName().substr(0,3) != "std")
			m->PrintJumps();
		if(printCode && m->GetName().substr(0,3) != "std")
			m->PrintCode();
	}
}



/*
 * Bank management and virtual address translation functions
 */

/*
 * Returns the address of the next virtual bank above the bank
 * containing the given virtual address.
 * Returns zero if there is no valid higher bank.
 */
unsigned int Compiler::GetNextBank(unsigned int adr)
{
	// Virtual banks are physically contiguous between two groups:
	//  [C0, FF], which are mapped to [00, 3F], and
	//  [40, 5F], which are mapped to [40, 5F].
	// However, we treat bank 40 as unusable, because some expanders will
	// copy the upper half of bank C0 into the upper half of bank 40 in
	// the process of 48Mbit ExHiROM expansion. I'm not entirely sure if
	// this is necessary for EarthBound, but better safe than sorry for now.
	int bank = (adr & 0xFF0000) >> 16;

	if(bank >= 0xC0 && bank < 0xFF)
		return (bank + 1) << 16;
	if(bank == 0xFF)
		return (0x41) << 16;
	if(bank >= 0x41 && bank < 0x5F)
		return (bank + 1) << 16;
	return 0;
}

/*
 * Returns the physical address corresponding to a given virtual address.
 * Returns 0xFBADF00D if the virtual address is invalid.
 */

unsigned int Compiler::MapVirtualAddress(unsigned int vadr)
{
	if(vadr >= 0xC00000 && vadr <= 0xFFFFFF)
		return vadr - 0xC00000 + (has_header? 0x200 : 0);
	if(vadr >= 0x400000 && vadr <= 0x5FFFFF)
		return vadr + (has_header? 0x200 : 0);
	return 0xFBADF00D;
}


/*
 * Registers a delayed write to the output file
 */
void Compiler::RegisterDelayedWrite(RomAccess* w)
{
	if(failed) return;
	romwrites.push_back(w);
}

/*
 * Performs all direct ROM access instructions registered
 */
void Compiler::DoDelayedWrites()
{
	if(failed) return;
	for(unsigned int i = 0; i < romwrites.size(); ++i)
	{
		// Resolve any references
		romwrites[i]->ResolveReferences();

		// Get the physical address of the write
		unsigned int padr = MapVirtualAddress(romwrites[i]->GetVirtualAddress());
		if(padr == 0xFBADF00D) {
			stringstream ss;
			ss << "error in ROM write statement: bad virtual address: " << std::setbase(16)
				<< romwrites[i]->GetVirtualAddress();
			throw Exception(ss.str());
		}
		romwrites[i]->DoWrite(filebuffer, padr, filesize);
	}
}


/*
 * Writes a "reset info" file, which contains information about the changes
 * the compiler made to the output file in this compilation. Specifically:
 *  - the range of primary compiled output
 *  - the previous contents of any ROM direct-write areas
 *
 * Format is text:
 * [start address] [end address]
 * {[hex address] [hex bytes] [newline]}
 *
 * e.g.,
 * F00000 F00412
 * F02000 0a 0b 0c 11 22 33
 * F15a02 00 00 00 00 00 00 00 00 00 00
 */
void Compiler::WriteResetInfo(const std::string &filename)
{
	ofstream file(ConvertToNativeString(filename));

	if(file.fail())
		throw Exception("couldn't create info file '" + filename + "'");

	file << std::hex << std::setfill('0');

	if(actual_start == -1 || actual_start == actual_end) {
		file << std::setw(6) << 0 << " " << 0 << std::endl;
	}
	else {
		file << std::setw(6) << actual_start
			 << " " << actual_end << std::endl;
	}

	for(vector<RomAccess*>::const_iterator it = romwrites.begin();
			it != romwrites.end(); ++it)
	{
		RomAccess* w = *it;

		unsigned int padr = MapVirtualAddress(w->GetVirtualAddress());
		unsigned int len = w->cache_value->GetSize();

		if(padr != 0xFBADF00D) {

			file << std::hex << std::setfill('0') << std::setw(6) << w->GetVirtualAddress() << " ";

			// Read data from the ROM at padr
			for(unsigned int i = 0; i < len; ++i) {
				unsigned int tmp = static_cast<unsigned char>(filebuffer[padr+i]);
				file << std::hex << std::setw(2) << tmp << " ";
			}

			file << std::endl;
		}
	}

	if(verbose)
		std::cerr << "Final output written from " << std::setbase(16) << actual_start
			<< " to " << actual_end << std::endl;

}

void Compiler::ApplyResetInfo(const std::string& filename)
{
	ifstream file(ConvertToNativeString(filename));

	if(file.fail())
		return;

	int start,end;

	// First read the 'clear' range
	file >> hex >> start >> end;

	if(verbose)
		std::cerr << "Zeroing previous output (" << std::setbase(16) << start << " to " << end << ")" << std::endl;
	start = Compiler::MapVirtualAddress(start);
	end = Compiler::MapVirtualAddress(end);
	for(int i = start; (i >= 0) && (i < filesize) && (i < end); i++)
		filebuffer[i] = 0;

	// Next restore whatever we had written via ROM[] statements previously
	while(!file.eof()) {
		string line;
		getline( file, line );

		if( line.empty()) continue;

		unsigned int vadr;
		stringstream ss(line);
		ss >> hex >> vadr;

		unsigned int padr = MapVirtualAddress( vadr );

		while(!ss.eof()) {
			unsigned int byte;
			ss >> byte;
			if(ss.fail()) break;
			filebuffer[padr++] = static_cast<unsigned char>(byte);
		}
	}
}



/*
 * Prints a summary of the number of errors and warnings issued, if any
 */
void Compiler::Results()
{
	if(!verbose && errorcount == 0 && warningcount == 0) return;
	std::cerr << std::endl << std::dec << errorcount << " error(s), " << warningcount << " warning(s)" << std::endl;
}


using std::endl;
using std::setbase;
using std::setw;
using std::setfill;
using std::left;

void Compiler::WriteSummary(std::ostream& out)
{
	out << filename << std::endl;
	out << "CCScript Compilation Summary" << endl;
	out << "============================" << endl;
	out << endl << endl;

	if(failed)
	{
		out << "COMPILATION FAILED";
			return;
	}
	//
	// Statistics
	//
	out << "Compilation statistics" << endl;
	out << "=================================================================" << endl;
	out << "Compilation start:           $" << setbase(16) << actual_start << endl;
	out << "Compilation end:             $" << setbase(16) << actual_end << endl;
	out << "Total compiled size:         " << setbase(10) << actual_end - actual_start << " bytes" << endl;
	out << "Fragmented space:            " << setbase(10) << totalfrag << " bytes" << endl;
	out << "-----------------------------------------------------------------" << endl;
	out << endl << endl;

	//
	// Module summaries
	//
	out << "Module information" << endl;
	out << "=================================================================" << endl;
	out << "Name                         Address     Size" << endl;
	out << "-----------------------------------------------------------------" << endl;
	for(unsigned int i = 0; i < modules.size(); ++i)
	{
		Module* m = modules[i];

		out << setfill(' ') << setw(29) << left << m->GetName() <<
			"$" << setw(12) << left << setbase(16) << m->GetBaseAddress() <<
			setw(6) << left << setbase(10) << m->GetCodeSize() << " bytes" << endl;
	}
	out << "-----------------------------------------------------------------" << endl;
	out << endl << endl;


	//
	// Label locations
	//
	out << "Label locations" << endl;
	out << "=================================================================" << endl << endl;
	for(unsigned int i = 0; i < modules.size(); ++i)
	{
		Module* m = modules[i];

		out << "Labels in module " << m->GetName() << endl;
		out << "Name                         Address" << endl;
		out << "-----------------------------------------------------------------" << endl;

		const std::map<std::string,Anchor*>& jumps = m->GetRootTable()->GetJumpTable();

		std::map<string,Anchor*>::const_iterator j;
		for(j = jumps.begin(); j != jumps.end(); ++j) {
			// Skip internal labels
			if(j->first.empty() || !isalpha(j->first.at(0)))
				continue;

			out << left << setw(28) << j->first << ' ';
			out << "$" << setbase(16) << j->second->GetTarget() << endl;
		}
		out << "-----------------------------------------------------------------" << endl;
		out << endl << endl;

	}
}

