/* CCScript tables */
#pragma once

// Tables in CCScript are the fundamental objects. A table is
// internally a hash table, as the name suggests. It is a binding
// of string identifiers to Values. Values can be numbers, strings,
// functions, or references to other tables.
// 
// When a CCScript class is instantiated, the result is a table with
// properties and methods derived from the class's prototype.
// 

struct Value;


class Table
{
public:
	Table();
	~Table();
};

