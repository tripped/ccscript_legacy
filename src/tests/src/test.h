///////////////////////////////////////////////////////////////////////////////
// test.h
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <exception>

// Exception class for unrecoverable errors
class fatal_error : public std::exception
{
	std::string message;
public:
	fatal_error(const char* msg)
	{
		message = msg;
	}
	fatal_error(const std::string& msg)
	{
		message = msg;
	}
	~fatal_error() throw() {
	}
	const char* what() const throw()
	{
		return message.c_str();
	}
};

// Test class
//  - encapsulates the loading and running of a test case
class Test
{
public:
	// Constructs a test case.
	//	filename -- name of the test script to run
	//	compiler -- path to the CCScript compiler
	//	testpath -- path in which to look for filename and any dependent files
	//	log      -- output stream for logging
	Test(const std::string& filename, const std::string& compiler, const std::string& testpath, std::ostream& log);

	// Runs the test. Returns true if the test succeeded, false otherwise.
	//	Throws runtime_error if a problem prevented this test from completing,
	//	or fatal_error if a problem requires that all tests be aborted.
	bool Run();


private:
	//
	// 'difference' structure used when comparing files
	//
	struct diff {
		unsigned int address;
		unsigned char result;
		unsigned char expected;
		diff(unsigned int a, unsigned char r, unsigned char e) {
			address = a; result = r; expected = e;
		}
	};

	//
	// Miscellaneous functions
	//
	std::string CreateCompilationFile(const std::string& name);
	int RunCompiler(const std::string& file, const std::string& options, /*out*/ std::string& output);
	bool CompareResults(const std::string& file, std::vector<Test::diff>& diffs, unsigned int maxdiffs);


	//
	// Inline data parsing
	//
	enum State
	{
		scanning, 
		in_quote, 
		in_code, 
		done 
	};
	void ParseCompData(const std::string& str);
	State DoString(const std::string& str, std::string::const_iterator& i);
	State DoCode(const std::string& str, std::string::const_iterator& i);
	State DoScan(const std::string& str, std::string::const_iterator& i);



private:
	Test(const Test&);
	Test& operator=(const Test&);


private:
	std::string filename;					// Name of test script being run
	std::string compiler;					// Path to CCScript compiler
	std::string testpath;					// Path of directory containing files for this test
	std::string name;						// Name of this test
	std::string desc;						// Test description
	std::string compilation_file;			// ROM filename for compilation
	std::string address;					// String specifying compilation address
	std::string expect_file;				// Filename containing expected output
	std::vector<unsigned char> expect_data;	// Vector containing expected output
	std::string expect_string;				// Original string representation of inline comparison data

	std::ostream& log;						// Logfile
};

