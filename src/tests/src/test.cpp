///////////////////////////////////////////////////////////////////////////////
// test.cpp

#include "test.h"
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#define popen _popen
#define pclose _pclose
#endif

#include "consolecolor.h"

using namespace std;

// Test construction

Test::Test(const string& filename, const string& compiler, const string& testpath, ostream& log)
	: filename(filename), compiler(compiler), testpath(testpath), log(log)
{
	string filepath = testpath + filename;
	ifstream file(filepath.c_str());

	if(file.fail())
		throw runtime_error("failed to open '" + filepath + "'");

	// Get all the metadata lines from the file
	vector<string> lines;
	while(!file.eof()) {
		string s;
		getline(file, s);
		if(s.substr(0, 3) == "///")
			lines.push_back(s.substr(3));
		else if(s.length() > 0)
			break;
	}
	file.close();

	// Parse out metadata
	// NOTE: this is ugly but it works.
	string expectline;
	bool folding_expect = false;
	for(vector<string>::iterator i = lines.begin(); i != lines.end(); ++i)
	{
		const string& line = *i;

		// The "expect" data is allowed to bleed into multiple lines for convenience
		if(folding_expect) {
			if(!line.empty() && line.substr(0,1) == "@") {
				folding_expect = false;
			}
			else {
				expectline += line;
				continue;
			}
		}

		if(line.substr(0,6) == "@name:") {
			name = line.substr(6);
		}
		else if(line.substr(0,6) == "@desc:") {
			desc = line.substr(6);
		}
		else if(line.substr(0,6) == "@file:") {
			compilation_file = line.substr(6);
		}
		else if(line.substr(0,6) == "@addr:") {
			address = line.substr(6);
		}
		else if(line.substr(0,8) == "@expect:") {
			expectline = line.substr(8);
			folding_expect = true;
		}
	}

	// Trim whitespace
	name.erase(0, name.find_first_not_of(" \t\r\n"));
	desc.erase(0, desc.find_first_not_of(" \t\r\n"));

	address.erase(0, address.find_first_not_of(" \t\r\n"));

	compilation_file.erase(0, compilation_file.find_first_not_of(" \t\r\n"));
	compilation_file.erase(compilation_file.find_last_not_of(" \t\r\n") + 1);

	expectline.erase(0, expectline.find_first_not_of(" \t\r\n"));
	expectline.erase(expectline.find_last_not_of(" \t\r\n") + 1);

	if(address.empty())
		address = "C00000";

	if(expectline.empty()) {
		throw runtime_error("no expected output data provided");
	}

	// Finally, convert the expect line into binary data
	// -- either directly or through a file

	// Assume that if the first character is not a quote mark that we want
	// to interpret expectline as the filename of the binary data to load
	if(expectline.at(0) != '"') {
		expect_file = expectline;
	}
	else {
		// Otherwise, we'll parse the text as follows: all text within matched
		// quotes will be interpreted as direct string data, with text in
		// brackets further interpreted as direct hex data.
		ParseCompData(expectline);
	}
}





//
// Runs the test, logging useful output. Returns true if the test succeeded,
// false otherwise.
//
bool Test::Run()
{
	log << "------------------------------------" << endl;
	log << "Test name:          " << name << endl;
	log << "Description:        " << desc << endl;
	log << "Expected output:    ";
	if(expect_file.empty()) log << "[listed in " << filename << "]" << endl;
	else log << expect_file << endl;

	//
	// First, generate the compilation file
	//
	string outfile = CreateCompilationFile("output.tmp");

	//
	// Invoke the compiler with the desired options
	//
	string options = " --printCode -o " + outfile + " -s " + address;
	string compiler_output;
	int retval = RunCompiler(filename, options, compiler_output);

	//
	// If the compiler fails to run or returns an error...
	//
	if(retval) {
		log << "Compile failure:" << endl;
		log << compiler_output << endl << endl;
		log << "Result: OMG TEST FAILURED" << endl << endl << endl;
		return false;
	}


	//
	// Finally, compare the contents of the output file to the expected data
	//
	vector<diff> diffs;
	bool ok = CompareResults("output.tmp", diffs, 11);

	if(ok)
		log << endl << "Result: TEST PASSED" << endl << endl;
	else {
		log << "Expected output: " << endl;
		log << expect_string << endl;
		log << "Actual output: " << endl;
		log << compiler_output << endl;
		log << "Differences:" << endl;
		log << "Address     Expected     Result     " << endl;
		log << "------------------------------------" << endl;
		for(unsigned int i = 0; i < diffs.size() && i < 10; ++i) {
			log << setw(6) << setfill(' ') << setbase(16) << diffs[i].address << "       ";
			log << setw(2) << setfill('0') << (int)diffs[i].expected << "          ";
			log << setw(2) << setfill('0') << (int)diffs[i].result << endl;
		}
		if(diffs.size() > 10)
			log << "More than 10 differences omitted..." << endl;
		log << endl << "Result: OMG TEST FAILURED" << endl << endl;
	}

	log << endl;

	return ok;
}




//
// Creates the file into which the test script will be compiled
//
string Test::CreateCompilationFile(const string& name)
{
	// All test files go in the test directory
	string file = testpath + name;

	ofstream out(file.c_str(), ios::binary | ios::trunc);
	if(out.fail())
		throw fatal_error(string("couldn't create temporary compilation file ") + file);

	if(compilation_file.empty()) {
		out.seekp(0x6001ff, ios::beg);
		out.put(0);
	}
	else {
		string compfile = testpath + compilation_file;
		ifstream in(compfile.c_str(), ios::binary);
		if(in.fail())
			throw runtime_error("couldn't open compilation file '" + compfile + "'");
		out << in.rdbuf();
		in.close();
	}
	out.close();

	return file;
}

int Test::RunCompiler(const string &file, const string &options, string& output)
{
	string command = compiler + " " + testpath + file + " " + options;
	log << "Command:            " << command << endl;
	command += " 2>&1";

	FILE* pipe = popen(command.c_str(), "r");

	string compiler_output;
	while(!feof(pipe)) {
		// For some reason I keep getting one garbage character
		// at the end of the stream
		char c = fgetc(pipe);
		if(!feof(pipe))
			output += c;
	}
	
	int retval = pclose(pipe);

	return retval;
}

//
// Checks the contents of the given file against expected results, storing
// any differences in the diffs vector, up to maxdiffs differences.
//
bool Test::CompareResults(const string& filename, vector<diff>& diffs, unsigned int maxdiffs)
{
	bool failed = false;

	string filepath = testpath + filename;

	ifstream result(filepath.c_str(), ios::binary);
	if(result.fail())
		throw fatal_error("couldn't open output file " + filepath);

	if(expect_file.empty()) {
		// If we're comparing against the inline data, begin the comparison at 0x200
		// (skip the header)
		result.seekg(0x200, ios::beg);

		istreambuf_iterator<char> eos;
		istreambuf_iterator<char> result_it(result.rdbuf());
		vector<unsigned char>::iterator expected_it = expect_data.begin();

		while(result_it != eos && expected_it != expect_data.end())
		{
			unsigned char r = *result_it;
			unsigned char e = *expected_it;

			if(r != e) {
				failed = true;
				if(diffs.size() < maxdiffs+1)
					diffs.push_back( diff( result.tellg(), r, e ) );
				else
					break;
			}
			++result_it;
			++expected_it;
		}
	}
	else
	{
		// Otherwise compare the whole file, as far as it goes
		string inpath = testpath + expect_file;
		ifstream expected(inpath.c_str(), ios::binary);
		if(expected.fail())
			throw runtime_error("couldn't open expected results file " + inpath);

		istreambuf_iterator<char> eos;
		istreambuf_iterator<char> result_it( result.rdbuf() );
		istreambuf_iterator<char> expect_it( expected.rdbuf() );

		while(result_it != eos && expect_it != eos)
		{
			unsigned char r = *result_it;
			unsigned char e = *expect_it;
			if(r != e) {
				failed = true;
				if( diffs.size() < maxdiffs+1 )
					diffs.push_back( diff( result.tellg(), r, e ) );
				else 
					break;
			}
			++result_it;
			++expect_it;
		}
	}
	return !failed;
}



//
// Private methods for parsing the "expected output" from the metadata fields
//
void Test::ParseCompData(const string& str)
{
	State state = scanning;
	string::const_iterator i = str.begin();
	while(state != done) {
		switch(state) {
			case in_quote:
				state = DoString(str, i);
				break;
			case in_code:
				state = DoCode(str, i);
				break;
			case scanning:
				state = DoScan(str, i);
				break;
			case done:
				break;
		}
	}
}


Test::State Test::DoString(const string& str, string::const_iterator& i)
{
	// We should not find the end of the string before finding an endquote
	if(i == str.end())
		throw runtime_error("unexpected end of data before terminating '\"'");

	char c = *i;
	++i;

	if(c == '"')
		return scanning;
	if(c == '[') {
		if(!expect_string.empty() && *(expect_string.end()-1) == ']') {
			expect_string.erase(expect_string.end()-1);
			expect_string += ' ';
		}
		else
			expect_string += '[';
		return in_code;
	}

	// Emit character and continue
	expect_data.push_back(c + 0x30);
	expect_string += c;

	return in_quote;
}

Test::State Test::DoCode(const string& str, string::const_iterator& i)
{
	// We shouldn't find the end of the string before a closing bracket is reached
	if(i == str.end())
		throw runtime_error("unexpected end of data before terminating ']'");
	if(*i == '"')
		throw runtime_error("unexpected '\"' in code segment");

	char byte[3] = {0, 0, 0};

	// consume leading whitespace
	while(isspace(*i))
		++i;

	if(*i == ']') {
		++i;
		if(!expect_string.empty() && *(expect_string.end()-1) == ' ')
			expect_string.erase(expect_string.end()-1);
		expect_string += ']';
		return in_quote;
	}

	char c = *i;

	// The next two characters must be hex digits
	for(int j = 0; j < 2; ++j) {
		if(!isxdigit(c)) {
			if(isspace(c) || c == ']')
				throw runtime_error(string("invalid hex sequence '") + byte + "': must have two digits");
			else
				throw runtime_error(string("unexpected character '") + c + "' in code segment");
		}

		byte[j] = c;
		++i; c = *i;
	}

	expect_data.push_back((unsigned char)strtoul(byte, NULL, 16));
	expect_string += byte;
	expect_string += " ";

	return in_code;
}

Test::State Test::DoScan(const string& str, string::const_iterator& i)
{
	if(i == str.end())
		return done;

	char c = *i;

	++i;

	if(isspace(c))
		return scanning;
	if(c == '"')
		return in_quote;

	// Any other character when scanning is an error
	throw runtime_error(string("unexpected character: '") + c + "'");
}
