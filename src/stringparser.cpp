/* implementation of ccscript strings */

#include "stringparser.h"
#include "ast.h"
#include "parser.h"
#include "module.h"
#include "bytechunk.h"

using std::string;


void StringParser::Error(const string &msg, int line_unused, int col)
{
	// We'll pass the error along to our internal error receiver,
	// but with a note added to indicate a string evaluation error
	error->Error(msg + " inside string", this->line, col);
}

void StringParser::Warning(const string &msg, int line_unused, int col)
{
	error->Warning(msg + " inside string", this->line, col);
}

int StringParser::acceptbyte()
{
	string s = "";

	if(!isxdigit(current)) return -1;
	s += current;
	next();
	if(!isxdigit(current)) return -1;
	s += current;
	//next();

	int n = strtoul(s.c_str(), NULL, 16);
	return n;
}

void StringParser::next()
{
	if(pos < str.length())
		current = str[pos++];
	else
		current = 0;
}

Value StringParser::Evaluate(SymbolTable* scope, EvalContext& context)
{
	String* output = new String();
	//ByteChunk* output = context.output;
	bool docodes = false;
	next();

	while(current != '\0') {
		// Handle '$' escapes
		if(current == '{') {
			output->Append( expression(scope, context).ToCodeString() );
			continue;
		}

		if(docodes) {
			// Break out of code mode
			if(current == ']') {
				next();
				docodes = false;
				continue;
			}
			// consume whitespace
			if(current == ' ' || current == '\t' || current == '\n') {
				next();
				continue;
			}
			int b = acceptbyte();
			if(b == -1)
				Warning(string("invalid control code bytes ignored"),0,0);
			else
				output->Byte(b);

			next();
		}
		else
		{
			if(current == '/') {
				output->Byte(16);
				output->Byte(5);
			}
			else if(current == '|') {
				output->Byte(16);
				output->Byte(15);
			}
			else if(current == '[') {
				docodes = true;
			}
			else {
				// Default:
				output->Char(current);
			}
			next();
			continue;
		}
	}

	return Value(output);
}


Value StringParser::expression(SymbolTable* scope, EvalContext& context)
{
	// Create a parser on just this section of the string
	size_t n = str.find('}', pos);

	if(n == string::npos) {
		Error(string("unterminated expression block"),0,0);
		pos = n;
		next();
		return Value();
	}

	string exstr = str.substr(pos, (n-pos));

	Parser parser(exstr);
	parser.SetErrorHandler(this);

	// Parse one expression and evaluate it
	Expression* e = parser.ParseExpression();
	Value result = e->Evaluate(scope, context);

	// Skip the expression block
	pos = n+1;
	next();

	return result;
}

