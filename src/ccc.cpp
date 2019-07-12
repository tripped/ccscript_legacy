// ccscript.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <codecvt>
#include <iterator>
#include <signal.h>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem::v1;

#include "ccc.h"
#include "compiler.h"
#include "module.h"
#include "util.h"

using std::vector;
using std::string;
using std::stringstream;
using std::cout;
using std::endl;



string getbasepath(const char* p)
{
	string path = p;
	size_t n = path.find_last_of("/\\");
	if(n == string::npos)
		return "";
	return path.substr(0, n) + "/";
}

void printversion()
{
	cout << "ccc version 1.339 Duck Tape Edition" << endl;
}

void printusage()
{
	cout << "Usage: ccc [options] [files] ... " << endl
		 << "Options: " << endl
		 << "   -o <file>             Dump compiled text into <file> at <address>" << endl
		 << "   -s,--start <adr>      Begin dumping at this address" << endl
		 << "   -e,--end <adr>        Do not write past this address" << endl
		 << "                           Addresses must be SNES offset, e.g., F00000" << endl
		 << "   -n,--no-reset         Do not use a 'reset' file to refresh ROM image" << endl
		 << "   --libs <path>         Look in <path> for all libraries" << endl
		 << "   --nostdlibs           Do not include the default standard libraries" << endl
		 << "   --summary <file>      Writes a compilation summary to <file>" << endl
		 << "                           Useful if you want to know where stuff went." << endl
		 //<< "   --shortpause <n>      Short pauses '/' are <n> frames long (default 5)" << endl
		 //<< "   --longpause <n>       Long pauses '|' are <n> frames long (default 15)" << endl
		 << "   --printAST            Prints the abstract syntax tree for each module" << endl
		 << "   --printRT             Prints the root symbol table for each module" << endl
		 << "   --printJumps          Prints the compiled addresses of all labels" << endl
		 << "   --printCode           Prints compiled code for each module" << endl
		 << "   -v                    Prints version number and exits" << endl
		 << endl
		 << "Example:" << endl
		 << endl
		 << "   ccc -o Earthbound.smc -s F20000 onett.ccs twoson.ccs threed.ccs" << endl
		 << endl
		 << "   This will compile onett.ccs, twoson.ccs, and threed.ccs together, and" << endl
		 << "   put the resulting compiled text at $F20000 in the ROM Earthbound.smc" << endl;
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;

	// Convert the utf-16 args to utf-8
	std::vector<std::string> utf8Args;
	for(int i = 0; i < argc; ++i) {
		auto utf8Arg = converter.to_bytes(argv[i]);
		utf8Args.push_back(utf8Arg);
	}
	
	// Expose the std::strings as a vector of const char*s
	std::vector<const char*> utf8Argv;
	std::transform(utf8Args.begin(), utf8Args.end(), std::back_inserter(utf8Argv),
              [](const std::string& s){ return s.c_str(); } );

	return run(argc, utf8Argv.data());
}
#else
int main(int argc, const char* argv[])
{
	return run(argc, argv);
}
#endif

int run(int argc, const char* argv[])
{
	//
	// Get the default libs path
	//
	string libspath = ( fs::path(argv[0]).parent_path() / "lib" ).string();


	if(argc < 2) {
		printusage();
		return -1;
	}


	string outfile;
	string startstr;
	string endstr;
	string summaryfile;
	unsigned long outadr = 0;
	unsigned long endadr = 0;
	vector<string> files;
	vector<string> libs;
	bool noreset = false;
	bool nostdlibs = false;
	unsigned char shortpause;
	unsigned char longpause;
	bool printAST = false;
	bool printRT = false;
	bool printJumps = false;
	bool printCode = false;
	bool verbose = false;

	// Command-line options:
	//  -o <file>			output filename
	//  -s <address>		output start
	//  -e <address>		output end
	//  -l <file>			include library file
	//  --libs <dir>         look in <dir> for standard libraries
	//  -h,--help			print help message
	//  --nostdlibs			do not include default libraries
	//  --shortpause <n>	duration of short pauses ('/')
	//  --longpause <n>		duration of long pauses ('|')
	//  --printAST			print AST for each module
	//  --printRT			print root table for each module
	//  --printJumps		print a list of jumps and addresses
	//  --printCode			print the code output for each module
	//  --summary <file>	output summary file
	//  --verbose			verbose output

	int p = 1;
	while(p < argc) {
		if(!strcmp(argv[p],"-o")) {
			p++;
			if(p >= argc) {
				std::cout << "argument error: no output file specified" << std::endl;
				return -1;
			}
			outfile = argv[p++];
		}
		else if(!strcmp(argv[p], "--start") || !strcmp(argv[p], "-s"))
		{
			p++;
			if(p >= argc) {
				std::cout << "argument error: no start address specified after -s" << std::endl;
				return -1;
			}
			startstr = argv[p++];
		}
		else if(!strcmp(argv[p], "--end") || !strcmp(argv[p], "-e"))
		{
			p++;
			if(p >= argc) {
				std::cout << "argument error: no end address specified after -e" << std::endl;
				return -1;
			}
			endstr = argv[p++];
		}
		else if(!strcmp(argv[p], "-v")) {
			printversion();
			return 0;
		}
		else if(!strcmp(argv[p],"--libs")) {
			p++;
			if(p >= argc) {
				std::cout << "argument error: no library path specified" << std::endl;
				return -1;
			}
			libspath = argv[p++];
		}
		else if(!strcmp(argv[p], "-h") || !strcmp(argv[p], "--help") || !strcmp(argv[p], "?")) {
			printusage();
			return 0;
		}
		else if(!strcmp(argv[p],"-l")) {
			p++;
			if(p >= argc) {
				std::cout << "argument error: no library file specified" << std::endl;
				return -1;
			}
			std::cerr << "WARNING: -l flag deprecated. Use 'import' statement to include libraries." << std::endl;
			libs.push_back(argv[p++]);
		}
		else if(!strcmp(argv[p],"-n") || !strcmp(argv[p], "--no-reset")) {
			p++;
			noreset = true;
		}
		else if(!strcmp(argv[p],"--nostdlibs")) {
			p++;
			nostdlibs = true;
		}
		else if(!strcmp(argv[p],"--summary") || !strcmp(argv[p], "--sum")) {
			p++;
			if(p >= argc) {
				std::cout << "argument error: no summary file specified" << std::endl;
				return -1;
			}
			summaryfile = argv[p++];
		}
		else if(!strcmp(argv[p],"--shortpause")) {
			p++;
			shortpause = (unsigned char)strtoul(argv[p++], NULL, 10);
		}
		else if(!strcmp(argv[p],"--longpause")) {
			p++;
			longpause = (unsigned char)strtoul(argv[p++], NULL, 10);
		}
		else if(!strcmp(argv[p],"--printAST")) {
			p++;
			printAST = true;
		}
		else if(!strcmp(argv[p],"--printRT")) {
			p++;
			printRT = true;
		}
		else if(!strcmp(argv[p],"--printJumps")) {
			p++;
			printJumps = true;
		}
		else if(!strcmp(argv[p],"--printCode")) {
			p++;
			printCode = true;
		}
		else if(!strcmp(argv[p],"--verbose"))
		{
			p++;
			verbose = true;
		}
		else {
			files.push_back(argv[p++]);
		}
	}



	if(!startstr.empty())
	{
		stringstream ss(startstr);
		ss >> std::setbase(16) >> outadr;
	}
	else
	{
		// Default to start of HiROM file
		outadr = 0xC00000;
	}

	if(!endstr.empty())
	{
		stringstream ss(endstr);
		ss >> std::setbase(16) >> endadr;
	}

	// Create compiler and set options
	Compiler compiler(outfile, outadr, endadr);
	compiler.printAST = printAST;
	compiler.printRT = printRT;
	compiler.printCode = printCode;
	compiler.printJumps = printJumps;
	compiler.verbose = verbose;
	compiler.libdir = libspath;
	compiler.noreset = noreset;
	compiler.nostdlibs = nostdlibs;

	/*
	 * 7/25/2009:
	 * Library loading has been fixed!
	 */

	for(unsigned int i = 0; i < libs.size(); ++i) {
		compiler.LoadModule(libs[i]);
	}

	for(unsigned int i = 0; i < files.size(); ++i) {
		compiler.LoadModule(files[i]);
	}

	// Do the stuff.
	compiler.Compile();
	compiler.WriteOutput();
	compiler.Results();

	// Write summary file
	if(!summaryfile.empty())
	{
		std::fstream file;
		file.open(ConvertToNativeString(summaryfile), std::ios_base::out|std::ios_base::trunc);
		if(file.fail())
		{
			std::cerr << "Couldn't open " << summaryfile << " to write summary file." << std::endl;
			return -1;
		}
		compiler.WriteSummary(file);
	}

	return compiler.Failed();
}