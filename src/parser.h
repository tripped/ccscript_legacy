/* ccscript parser */
#pragma once

#include <string>
#include <map>
#include <vector>

#include "lexer.h"

class Node;
class Statement;
class Expression;
class Program;
class IfExpr;
class MenuExpr;
class CountExpr;
class CommandDef;
class ConstDef;
class VarDef;

/*
 * A simple recursive descent parser based on ccscript's LL(2) grammar.
 * Verifies that a program is derivable under the grammar, and if so,
 * produces an abstract syntax tree representation of the program.
 */
class Parser
{
public:
	Parser(Lexer*);
	Parser(const std::string& src);
	void SetErrorHandler(ErrorReceiver*);
	Program* Parse();
	Expression* ParseExpression();
	int line;
	
private:
	Lexer *lexer;

	Token last;
	symbol sym;
	void getsym ();
	bool expect (symbol s);
	bool accept (symbol s);

	Program* program();
	Statement* statement();
	Expression* expression();
	Expression* primaryexpr();
	Expression* boundedexpr();
	Expression* factor();
	IfExpr* ifexpr();
	MenuExpr* menuexpr();
	CountExpr* countexpr();
	CountExpr* setcountexpr();
	CommandDef* commanddef();
	ConstDef* constdef();

	ErrorReceiver* error;
	void Error(const std::string &msg, int line);
	void Warning(const std::string &msg, int line);
};

