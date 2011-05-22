/* ccscript lexical analyzer/scanner */
#pragma once

#include <string>
#include <map>
#include <cstdlib>
#include <cstring>
#include "err.h"

class Lexer;


typedef enum {
	finished = 0,
	identifier, intliteral, stringliteral, constsym, flagsym,
	ifsym, elsesym, menusym, defaultsym, commandsym, andsym, orsym, notsym,
	bytesym, shortsym, longsym, romsym, romtblsym,
	leftparen, rightparen, leftbrace, rightbrace, leftbracket,
	rightbracket, period, colon, comma, equals,
	importsym,
	countsym, setcountsym,
	errorsym
} symbol;


struct Token
{
	symbol	sym;			// the type of token
	int		line;			// the source line on which this token occurs
	int		ival;			// integer value of the token
	std::string	sval;		// string value of the token
	char	stype;			// for strings: indicates type of string literal
	
	std::string	ToString();
};


class Lexer
{
public:
	int line;				// current line being scanned
	int column;				// character column reached

	symbol currentsym;		// last-lexed token symbol
	int currentint;			// integer value of last-lexed symbol
	std::string currentstr;	// string value of last-lexed symbol
	char currentstype;		// type of string, if last symbol was a string

	enum charcode {
		eob = '\0'
	};

public:
	Lexer(std::string);		// Constructs lexer on given input
	symbol Lex();			// Reads next symbol from input; returns `finished` when there are no more tokens
	symbol Peek();			// Checks the next symbol without advancing or modifying current value.
							//  Just a quick hack to ease parsing of LL(2) grammar :-)

	void GetCurrentToken(Token&) const;	// Gets the last-read token value
	Token GetCurrentToken() const;
	int GetPosition() const;			// Returns the current position within the input
	void SetErrorHandler(ErrorReceiver* e);
	void AddKeyword(const std::string& kw, symbol sym);

	static std::string SymbolToString(symbol sym);

private:
	void Init();
	void Next();
	symbol LexSymbol();
	symbol LexStringLiteral();
	symbol LexIdentifier();
	symbol LexNumber();
	void LexSingleComment();
	bool LexBlockComment();
	void Error(const std::string& msg);
	void Warning(const std::string& msg);

	ErrorReceiver *error;

	std::map<std::string, symbol> keywords;
	std::string in;
	size_t inpos;
	char current;
};

