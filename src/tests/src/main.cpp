///////////////////////////////////////////////////////////////
// A simple test framework for CCScript
//
// This program allows for the input of a number of test scripts
// along with expected output data; it will compile the provided
// files and check the output against the expected, printing a
// list of results for each test, including a list of differences
// found.
//
// See readme.txt for details on specifying test cases.
//
#include <iostream>
#include <string>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <stdlib.h>
#include <time.h>

using namespace std;

#include "test.h"
#include "consolecolor.h"

int main(int argc, char** argv)
{
	if(argc < 3) {
		cout << "Usage: " << endl
			 << "   tests <PathToCCC> <TestListFile>" << endl;
		return 1;
	}


	string compiler_path = argv[1];
	string tests_file(argv[2]);

	int total_tests = 0;
	int failed = 0;
	int skipped = 0;

	//
	// Get the path, relative to the working directory, of the
	// directory containing the tests file
	//
	string testlist_dir(tests_file);
	if(testlist_dir.find_last_of("\\/") == string::npos)
		testlist_dir = "";
	else
		testlist_dir.erase( testlist_dir.find_last_of("\\/") );



	//
	// Open logfile and output some general info
	//
	string logfile = testlist_dir + "/tests.log";
	ofstream log(logfile.c_str());
	if(log.fail()) {
		cerr << "couldn't create " << logfile << endl;
		return 1;
	}
	time_t tt = time(NULL);
	log << "CCScript Integration Tests" << endl;
	log << "==========================" << endl;
	log << "" << endl;
	log << "Run on: " << asctime(localtime(&tt));
	log << "" << endl;
	log << "CCScript version information:" << endl;
	log.close();
	// Um, this is a horrible and yet hilarious hack
	if(system(string(compiler_path + " -v >> " + logfile).c_str())) {
		cerr << "couldn't run " << compiler_path << ", aborting" << endl;
		return 1;
	}
	log.open(logfile.c_str(), ios::app);
	log << endl << "==========================" << endl;
	log << endl << "BEGIN TESTS" << endl << endl;


	//
	// Try to run all tests specified in the test list file
	//
	try
	{
		ifstream file(tests_file.c_str());
		if(file.fail())
			throw runtime_error(string("couldn't open test list file '") + tests_file + "'");

		while(!file.eof())
		{
			string testfilename;
			getline(file, testfilename);

			// Trim leading whitespace; ignore lines beginning with '//'
			testfilename.erase(0, testfilename.find_first_not_of(" \r\n\t"));
			testfilename.erase(testfilename.find_last_not_of(" \r\n\t")+1);
			if(testfilename.empty() || testfilename.substr(0,2) == "//")
				continue;

			try
			{
				++total_tests;

				// Each test should look for and create any dependent files
				// in the same directory as the test script file; to get this
				// path relative to the working directory, we add the relative
				// path portion of the test file path to the path of the test
				// list file.
				//
				// NOTE: this assumes that all the paths in the test list file
				// are relative, which is fine for our purposes. For anything
				// more general, we should just use the standard library's filesystem path...
				string test_dir( testfilename );
				string::size_type lastsep = test_dir.find_last_of("\\/");
				if(lastsep == string::npos)
					test_dir = "";
				else
					test_dir.erase(lastsep);
				if(!testlist_dir.empty())
					test_dir = testlist_dir + '/' + test_dir;

				cout << "Running test " << setw(32) << left << (testfilename + "...") << right;

				Test test(testfilename, compiler_path, test_dir, log);

				bool succeeded = test.Run();

				if(succeeded) {
					cout << color::green << setw(7) << " [OK]" << color::normal << endl;
				}
				else {
					cout << color::red << setw(7) << " [FAIL]" << color::normal << endl;
					++failed;
				}
			}
			catch(runtime_error& e)
			{
				cout << color::yellow << setw(7) << " [SKIP]" << color::normal << endl;
				cerr << "  warning: " << e.what() << endl;
				cerr << "  skipping test " << testfilename << endl;
				++skipped;
			}
		}
	}
	catch(exception& e) {
		cerr << "error: " << e.what() << endl;
		cerr << "aborting tests." << endl;
	}
	catch(...) {
		cerr << "an unexpected exception occurred." << endl;
		cerr << "aborting tests." << endl;
	}

	//
	// Output brief summary
	//
	cout << "   " << endl;
	cout << "   " << (total_tests - failed - skipped) << "/" << total_tests << " tests passed!" << endl;
	if(failed) cout << "   " << failed << " tests failed." << endl;
	if(skipped) cout << "   " << skipped << " tests skipped." << endl;

	return 0;
}

