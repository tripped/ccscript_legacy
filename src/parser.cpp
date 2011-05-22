/* ccscript parser */
#include "parser.h"

#include "lexer.h"
#include "ast.h"

#include <iostream>
#include <vector>

using namespace std;

/*
 * Public interface
 */
Parser::Parser(Lexer* lexer)
{
	this->lexer = lexer;
	error = NULL;
	line = 0;
}

Parser::Parser(const std::string& src)
{
	this->lexer = new Lexer(src);
	error = NULL;
	line = 0;
}

void Parser::SetErrorHandler(ErrorReceiver* e)
{
	this->error = e;
	this->lexer->SetErrorHandler(e);
}

Program* Parser::Parse()
{
	return program();
}

Expression* Parser::ParseExpression()
{
	getsym();
	return expression();
}


/*
 * Private methods
 */

void Parser::Error(const string &msg, int line)
{
	if(error) error->Error(msg, line, -1);
}

void Parser::Warning(const string &msg, int line)
{
	if(error) error->Warning(msg, line, -1);
}

void Parser::getsym() {
	last = lexer->GetCurrentToken();
	sym = lexer->Lex();
	line = lexer->line;
}

bool Parser::accept(symbol s) {
	if(sym == s) {
		getsym();
		//printf("ACCEPTING %s, next up: %s\n", get_sym(s).c_str(), get_sym(sym).c_str());
		return true;
	}
	return false;
}

bool Parser::expect(symbol s) {
	if(accept(s))
		return true;
	stringstream ss;
	ss << "expected '" << Lexer::SymbolToString(s) << "', found '"
		<< Lexer::SymbolToString(sym) << "'";
	Error(ss.str(), line);
	return false;
}


/********************
 * Production rules *
 ********************/

/*
 *  program := ( stmt )* EOF
 */
Program* Parser::program() {
	getsym();	// prime the pump

	Program* p = new Program(0, error);

	while(sym != finished) {

		if(accept(importsym)) {
			
			if(accept(identifier)) {
				p->imports.push_back(last.sval + ".ccs");
			}
			else {
				expect(stringliteral);
				p->imports.push_back(last.sval);
			}
		}
		else {
			p->Add(statement());
		}
	}
	return p;
}


/*
 *  stmt := '{' (stmt)* '}'
 *		   | command-def
 *		   | const-def
 *         | variable-def
 *		   | expr
 */
Statement* Parser::statement() {
	// Always check for block statements first! Block statements
	// have a weird property: they can be interpreted as expressions
	// in the expression() production rule.
	if(accept(leftbrace)) {
		Block* b = new Block(last.line, error);
		while(sym != rightbrace && sym != finished) {
			b->Add(statement());
		}
		expect(rightbrace);
		return b;
	}
	else if(accept(commandsym)) {
		return commanddef();
	}
	else if(accept(constsym)) {
		return constdef();
	}

	// ROM writing statements
	else if(accept(romsym)) {
		RomWrite* stmt = new RomWrite(last.line, error);
		expect(leftbracket);
		stmt->SetBase(expression());
		expect(rightbracket);
		expect(equals);
		stmt->SetValue(expression());
		return stmt;
	}
	else if(accept(romtblsym)) {
		RomWrite* stmt = new RomWrite(last.line, error);
		expect(leftbracket);
		stmt->SetBase(expression());
		expect(comma);
		stmt->SetSize(expression());
		expect(comma);
		stmt->SetIndex(expression());
		expect(rightbracket);
		expect(equals);
		stmt->SetValue(expression());
		return stmt;
	}
	// Identifiers can also begin expressions, so we have to
	// peek ahead to decide which production rule to apply
	/*else if(sym == identifier) {
		symbol peeked = lexer->Peek();
		if(peeked == colon) {
			accept(identifier);
			int line = last.line;
			string name = last.sval;
			accept(colon);
			return new Label(line, name, error);
		}
		else {
			return new ExprStmt(last.line, expression(), error);
		}
	}*/
	// If all else fails, try it as an expression statement
	return new ExprStmt(last.line, expression(), error);
}

/*
 *  if-expr := 'if' cond-expr then-expr tail-if
 *  tail-if := nil | 'else' else-expr
 */
IfExpr* Parser::ifexpr() {
	int line = last.line;
	Expression* cond = expression();
	Expression* thenexpr = expression();
	Expression* elseexpr = NULL;
	if(accept(elsesym)) {
		elseexpr = expression();
	}
	return new IfExpr(line, cond, thenexpr, elseexpr, error);
}

/*
 *  menu-expr := 'menu' '{' ( [ 'default' ] opt-expr ':' result-expr) * '}'
 */
MenuExpr* Parser::menuexpr() {
	MenuExpr* menu = new MenuExpr(last.line, error);

	// Columns override
	int cols = -1;
	if(accept(intliteral)) {
		cols = last.ival;
	}

	expect(leftbrace);
	int opts = 0;
	while(sym != rightbrace && sym != finished) {
		if(accept(defaultsym)) {
			menu->SetDefault(opts);
		}
		Expression* option = expression();
		expect(colon);
		Expression* result = expression();
		menu->Add(option, result);
		opts++;
	}
	expect(rightbrace);

	if(cols != -1)
		menu->SetColumns(cols);
	return menu;
}

/*
 *  command-def := 'command' ident stmt
 */
CommandDef* Parser::commanddef() {
	// save the line containing the "command" keyword
	int line = last.line;
	//if(expect(identifier)) {
	expect(identifier);
		CommandDef* cmd = new CommandDef(line, last.sval, error);

		// Read argument definitions, if any
		if(accept(leftparen)) {
			// Initial argument
			if(sym != rightparen) {
				expect(identifier);
				cmd->AddArg(last.sval);
			}
			// Subsequent arguments must be preceded by a comma
			while(sym != rightparen && sym != finished) {
				if(!expect(comma)) break;
				if(!expect(identifier)) break;
				cmd->AddArg(last.sval);
			}
			// Don't forget the closing parenthesis!
			expect(rightparen);
		}

		Expression* body = expression();

		// DUCT TAPE:
		if(body->GetType() == blockexpr) {
			(dynamic_cast<BlockExpr*>(body))->NoLocalScope(true);
		}

		cmd->SetBody(body);
		return cmd;
	//}
	
	//return new ErrorExpr(line, "invalid command def");
}

/*
 *  const-def := 'const' ident '=' expression
 */
ConstDef* Parser::constdef() {
	int line = last.line;
	expect(identifier);
	string name = last.sval;
	expect(equals);
	return new ConstDef(line, name, expression(), error);
}


/*
 *  expr := ifexpr				; ifs and menus are now expressions, not statements
 *		   | menuexpr			; 
 *		   | blockexpr			; block statements can be interpreted as expressions
 *         | labelexpr			; labels are now expressions, not statements
 *		   | fac 'and' expr
 *		   | fac 'or' expr
 *		   | fac
 *		   | 'byte' expr
 *		   | 'short' expr
 */
Expression* Parser::expression()
{
	if(accept(ifsym))
		return ifexpr();

	if(accept(menusym))
		return menuexpr();

	// Peek ahead for labels here now
	if(sym == identifier) {
		symbol peeked = lexer->Peek();
		if(peeked == colon) {
			accept(identifier);
			int line = last.line;
			string name = last.sval;
			accept(colon);
			return new Label(line, name, error);
		}
	}

	// Block statements can be treated as expressiosn.
	// But don't eat up the '{'! block-stmt wants it.
	if(sym == leftbrace)
	{
		Block* b = dynamic_cast<Block*>(statement());
		return new BlockExpr(last.line, b, error);
	}

	// Note that we don't 'accept()' the next symbol here because
	// boundedexpr() needs it to figure out what kind of bounded
	// expression to create.
	// TODO: give boundedexpr() a parameter specifying this. :)
	if(sym == bytesym || sym == shortsym || sym == longsym)
		return boundedexpr();

	int line = last.line;
	Expression* exp1 = factor();
	if(accept(andsym)) {
		Expression* exp2 = expression();
		return new AndExpr(line, exp1, exp2, error);
	}
	else if(accept(orsym)) {
		Expression* exp2 = expression();
		return new OrExpr(line, exp1, exp2, error);
	}
	return exp1;
}

/*
 *  boundedexp := sizetag [ '[' INT_LITERAL ']' ] expr
 */
Expression* Parser::boundedexpr() {
	int size = -1;
	if(accept(bytesym)) size = 1;
	else if(accept(shortsym)) size = 2;
	else if(accept(longsym)) size = 4;

	BoundedExpr* ex = new BoundedExpr(last.line, size, error);

	// Check for an indexer
	if(accept(leftbracket)) {
		expect(intliteral);
		ex->SetIndex(last.ival);
		expect(rightbracket);
	}

	ex->SetExpr(expression());

	return ex;
}

CountExpr* Parser::countexpr() {
	int line = last.line;
	string id;
	int offset = 0;
	int multiple = 1;

	expect(leftparen);
	expect(stringliteral);

	id = last.sval;

	if(accept(comma)) {
		expect(intliteral);
		offset = last.ival;
		if(accept(comma)) {
			expect(intliteral);
			multiple = last.ival;
		}
	}
	expect(rightparen);

	return new CountExpr(line, id, offset, multiple, error);
}

CountExpr* Parser::setcountexpr() {
	int line = last.line;
	string id;
	int value = 0;

	expect(leftparen);
	expect(stringliteral);
		id = last.sval;
	expect(comma);
	expect(intliteral);
		value = last.ival;
	expect(rightparen);

	// Horrible, horrible. This should really be two classes,
	// but it's 2:56 AM and this is a duct-tape feature that
	// will under no circumstances be maintained.
	return new CountExpr(line, id, value, error);
}


/*
 *  fac := primary-expr
 *         | 'flag' primary-expr
 *		   | '(' expr ')'
 *		   | 'not' factor
 */
Expression* Parser::factor() {
	if(accept(flagsym)) {
		int line = last.line;
		return new FlagExpr(line, primaryexpr(), error);
	}

	else if(accept(leftparen)) {
		Expression* expr = expression();
		expect(rightparen);
		return expr;
	}

	// This goes in 'factor' because we want it to associate tightly
	else if(accept(notsym)) {
		return new NotExpr(last.line, factor(), error);
	}

	// Otherwise, expect a primary expression
	return primaryexpr();
}


/*
 *  primary-expr :=
 *         | INT_LITERAL
 *         | STRING_LITERAL
 *		   | ident ['(' ( expr ( ',' expr )* )? ')']
 *         | count-expr
 */
Expression* Parser::primaryexpr() {
	if(accept(countsym))
		return countexpr();
	if(accept(setcountsym))
		return setcountexpr();
	if(accept(intliteral))
		return new IntLiteral(last.line, last.ival, error);
	if(accept(stringliteral))
		return new StringLiteral(last.line, last.sval, error);

	if(accept(identifier)) {
		int line = last.line;
		string file = "";
		string name = last.sval;

		// Check for an external namespace reference
		if(accept(period)) {
			file = name;
			expect(identifier);
			name = last.sval;
		}

		IdentExpr* id = new IdentExpr(line, file, name, error);

		// Add arguments if we find a '('
		if(accept(leftparen)) {
			id->UseParens();
			if(sym != rightparen)
				id->AddArg(expression());
			while(sym != rightparen && sym != finished) {
				expect(comma);
				id->AddArg(expression());
			}
			expect(rightparen);
		}
		return id;
	}

	// freeeow.
	getsym();
	stringstream ss;
	ss << "symbol '" << last.ToString() << "'";
	Error(ss.str(), last.line);
	return new ErrorExpr(last.line, "unexpected symbol '" + last.ToString() + "'");
}