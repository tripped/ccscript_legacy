/* CCScript lexical analyzer/scanner */


#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <errno.h>
#include "lexer.h"

using namespace std;

Lexer::Lexer(string src)
{
	in = src;
	inpos = 0;
	Init();
}

void Lexer::Init()
{
	error = NULL;
	AddKeyword("if",ifsym);
	AddKeyword("else",elsesym);
	AddKeyword("menu",menusym);
	AddKeyword("default",defaultsym);
	AddKeyword("define",constsym);
	AddKeyword("command",commandsym);
	AddKeyword("or", orsym);
	AddKeyword("and", andsym);
	AddKeyword("not", notsym);
	AddKeyword("flag", flagsym);
	AddKeyword("byte", bytesym);
	AddKeyword("short", shortsym);
	AddKeyword("long", longsym);
	AddKeyword("ROM", romsym);
	AddKeyword("ROMTBL", romtblsym);
	AddKeyword("import", importsym);
	AddKeyword("count", countsym);
	AddKeyword("setcount", setcountsym);
	line = 1;
	column = 0;
	currentsym = errorsym;
	Next();
}

void Lexer::SetErrorHandler(ErrorReceiver* e)
{
	error = e;
}

void Lexer::Error(const string& msg)
{
	if(error) error->Error(msg, line, column);
}

void Lexer::Warning(const string& msg)
{
	if(error) error->Warning(msg, line, column);
}

void Lexer::AddKeyword(const string& kw, symbol sym)
{
	keywords[kw] = sym;
}

/*static*/ string Lexer::SymbolToString(symbol sym)
{
	switch(sym) {
		case finished: return "end of file";
		case identifier: return "identifier";
		case intliteral: return "int literal";
		case stringliteral: return "string literal";
		case constsym: return "define";
		case flagsym: return "flag";
		case ifsym: return "if";
		case elsesym: return "else";
		case menusym: return "menu";
		case defaultsym: return "default";
		case commandsym: return "command";
		case andsym: return "and";
		case orsym: return "or";
		case notsym: return "not";
		case bytesym: return "byte";
		case shortsym: return "short";
		case longsym: return "long";
		case romsym: return "ROM";
		case romtblsym: return "ROMTBL";
		case leftparen: return "(";
		case rightparen: return ")";
		case leftbrace: return "{";
		case rightbrace: return "}";
		case leftbracket: return "[";
		case rightbracket: return "]";
		case period: return ".";
		case colon: return ":";
		case comma: return ",";
		case equals: return "=";
		case importsym: return "import";
		case countsym: return "count";
		case setcountsym: return "setcount";
		default: return "INVALID SYMBOL";
	}
}

int Lexer::GetPosition() const
{
	return inpos;
}

void Lexer::GetCurrentToken(Token& t) const
{
	t.sym = currentsym;
	t.line = line;
	t.ival = currentint;
	t.sval = currentstr;
	t.stype = currentstype;
}


Token Lexer::GetCurrentToken() const
{
	Token t;
	GetCurrentToken(t);
	return t;
}

std::string Token::ToString()
{
	switch(sym) {
		case stringliteral:
			return stype + string("\"") + sval + '\"';
		case leftparen: return "(";
		case rightparen: return ")";
		case leftbrace: return "{";
		case rightbrace: return "}";
		case period: return ".";
		case comma: return ",";
		case colon: return ":";
		case equals: return "=";
		case finished:
		case errorsym:
			return "INVALID_TOKEN";
		default:
			return sval;
	}
}

void Lexer::Next()
{
	if(inpos >= in.length())
		current = eob;
	else {
		current = in.at(inpos++);
		column++;
	}
}

void Lexer::LexSingleComment()
{
	do {
		Next();
	} while(current != '\n' && current != eob);
}

bool Lexer::LexBlockComment()
{
	Next();
	for(;;) {
		switch(current) {
			case '*':
				Next(); // Check next character for comment terminator
				if(current == '/') { Next(); return true; }
				continue;
			case '\n':
				line++;
				Next();
				continue;
			case eob:
				Error("unexpected end of file in comment");
				return false;	// fix infinite loop in unclosed comment :P
			default:
				Next();
		}
	}
	return true;
}

symbol Lexer::LexStringLiteral()
{
	currentstr = "";

	while(current != '\"')
	{
		switch(current) {
			case eob:
				Error("unexpected end of file in string literal");
				return errorsym;
			case '\n':
				Error("newline in string");
				line++;
				return errorsym;
			case '\\':
				Next();
				switch(current) {
				case '\"': Next(); currentstr += '\"'; break;
				case '\\': Next(); currentstr += '\\'; break;
				default:
					Next();
					Warning("unrecognized escape character ignored");
					continue;
				}
			default:
				currentstr += current;
				Next();
		}
	}

	Next();
	return stringliteral;
}

symbol Lexer::LexIdentifier()
{
	currentstr = "";

	do {
		currentstr += current;
		Next();
	} while(isalnum(current) || current == '_');

	if(keywords.find(currentstr) != keywords.end()) {
		return keywords[currentstr];
	}

	return identifier;
}

symbol Lexer::LexNumber()
{
	char first = current;
	int radix = 0;
	bool negate = false;
	currentstr = "";

	if(current == '-') {
		negate = true;
		Next();
		first = current;
	}

	Next();

	if(first == '0' && toupper(current) == 'X') {
		currentstr += first;
		currentstr += current;
		radix = 16;
		Next();
		while(isxdigit(current)) {
			currentstr += current;
			Next();
		}
	}
	else {
		radix = 10;
		currentstr += first;
		while(isdigit(current)) {
			currentstr += current;
			Next();
		}
	}

	if(isalnum(current)) {
		Error("number has invalid suffix");
	}
	unsigned int temp = 0;
	stringstream ss(currentstr);
	ss >> setbase(radix) >> temp;
	if(ss.fail()) {
		Warning("integer constant capped at 0xffffffff");
		temp = 0xffffffff;
	}
	currentint = temp;

	if(negate)
		currentint = -currentint;

	return intliteral;
}

symbol Lexer::LexSymbol()
{
	while(current != eob)
	{
		switch(current)
		{
		case '\t': case '\r': case ' ':
			Next();
			continue;

		case '\n':
			line++;
			column = 1;
			Next();
			continue;

		case '/':
			Next();
			switch(current) {
			case '/': LexSingleComment(); continue;
			case '*': if(!LexBlockComment()) return errorsym; continue;
			default:
				Error("unexpected character '/'");
				continue;
			}

		case '!':
		case '~':
			currentstype = current;
			Next();
			if(current != '\"') {
				Error("string expected");
				return errorsym;
			}
			Next();
			return LexStringLiteral();

		case '\"':
			currentstype = ' ';
			Next();
			return LexStringLiteral();

		case '=': Next(); return equals;
		case '(': Next(); return leftparen;
		case ')': Next(); return rightparen;
		case '{': Next(); return leftbrace;
		case '}': Next(); return rightbrace;
		case '[': Next(); return leftbracket;
		case ']': Next(); return rightbracket;
		case '.': Next(); return period;
		case ',': Next(); return comma;
		case ':': Next(); return colon;

		default:
			if(isalpha(current) || current == '_') {
				return LexIdentifier();
			}
			else if(isdigit(current) || current == '-') {
				return LexNumber();
			}
			else {
				Error(string("unexpected character '") + current + "'");
				Next();
				continue;
			}
		}
	}
	return finished;
}

symbol Lexer::Lex()
{
	currentsym = LexSymbol();
	return currentsym;
}

symbol Lexer::Peek()
{
	// Not the most elegant way to peek, but it works...

	int oldint = currentint;
	string oldstr = currentstr;
	char oldstype = currentstype;
	int oldline = line;
	int oldcolumn = column;
	int oldpos = inpos;
	char oldc = current;

	symbol sym = LexSymbol();
	
	if(sym != errorsym) {
		currentint = oldint;
		currentstr = oldstr;
		currentstype = oldstype;
		line = oldline;
		column = oldcolumn;
		inpos = oldpos;
		current = oldc;
	}

	return sym;
}

