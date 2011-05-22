/* symbol table implementation */

#include "symboltable.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include "anchor.h"
#include "ast.h"
#include "value.h"

using namespace std;


SymbolTable::~SymbolTable()
{
	// Clean up only the labels
	/*map<string,Anchor*>::iterator i;
	for(i = jumps.begin(); i != jumps.end(); ++i) {
		delete i->second;
	}*/

	// Actually, don't even clean up the labels.
	// This whole setup is a pile of crap, but in the short
	// term it doesn't matter so much if we just leak the
	// labels. This is just a stopgap release until Banana
	// is in working order, and in Banana at least we use
	// proper memory management...
}

// Copy constructor
SymbolTable::SymbolTable(const SymbolTable& other)
{
	table = other.table;
	jumps = other.jumps;
}


/*
 * Merges the contents of another symbol table into this one.
 * Populates 'collisions' with the names of any identifiers that collide
 * with already-defined ones.
 */
void SymbolTable::Merge(const SymbolTable &other, vector<string> &collisions)
{
	for(map<string,Value>::const_iterator
			it = other.table.begin(); it != other.table.end(); ++it)
	{
		if( Lookup(it->first) == Value::Undefined && LookupAnchor(it->first) == NULL )
		{
			Define(it->first, it->second);
		}
		else
			collisions.push_back(it->first);
	}

	for(map<string,Anchor*>::const_iterator
			it = other.jumps.begin(); it != other.jumps.end(); ++it)
	{
		if( Lookup(it->first) == Value::Undefined && LookupAnchor(it->first) == NULL )
			DefineAnchor(it->first, it->second);
		else
			collisions.push_back(it->first);
	}
}


/*
 * Sets a parent symbol table for scope chaining
 */
void SymbolTable::SetParent(SymbolTable* parent)
{
	this->parent = parent;
}


/*
 * Adds a base address to the targets of all labels defined in the table
 */
void SymbolTable::AddBaseAddress(unsigned int base)
{
	map<string,Anchor*>::iterator i;
	for(i = jumps.begin(); i != jumps.end(); ++i) {
		Anchor* lbl = i->second;
		lbl->SetTarget(lbl->GetTarget() + base);
	}
}

/*
 * Maps a symbol name to a value.
 */
bool SymbolTable::Define(const string& name, Value val)
{
	bool retval = false;
	if(table.find(name) != table.end())
		retval = true;
	table[name] = val;
	return retval;
}

/*
 * Defines a label. Returns true if the label's name was already mapped.
 */
bool SymbolTable::DefineAnchor(Anchor* a)
{
	return DefineAnchor(a->GetName(), a);
}

/*
 * Defines a label mapped to the specified name.
 * Returns true if the name was already mapped.
 */
bool SymbolTable::DefineAnchor(const string& name, Anchor* label)
{
	bool retval = false;
	if(jumps.find(name) != jumps.end())
		retval = true;
	jumps[name] = label;
	return retval;
}

/*
 * Looks up the given symbol in this table and all parent tables,
 * returning the node associated with the first mapping found,
 * or NULL if the symbol is not defined.
 */
Value SymbolTable::Lookup(const string& name) const
{
	map<string,Value>::const_iterator f = table.find(name);
	if(f == table.end()) {
		if(parent) return parent->Lookup(name);
		else return Value::Undefined;
	}
	return f->second;
}

Value SymbolTable::Get(const string& name) const
{
	map<string,Value>::const_iterator f = table.find(name);
	if(f == table.end())
		return Value::Undefined;
	return f->second;
}

/*
 * Looks up a label name in this and all parent tables, returning
 * the label associated with the first mapping found, or NULL if
 * no label is defined with the given name.
 */
Anchor* SymbolTable::LookupAnchor(const string& name) const
{
	map<string,Anchor*>::const_iterator f = jumps.find(name);
	if(f == jumps.end()) {
		if(parent) return parent->LookupAnchor(name);
		else return NULL;
	}
	return f->second;
}

Anchor* SymbolTable::GetAnchor(const string& name) const
{
	map<string,Anchor*>::const_iterator f = jumps.find(name);
	if(f == jumps.end())
		return NULL;
	return f->second;
}

/*
 * Returns a string representation of the symbol table.
 */
string SymbolTable::ToString() const
{
	stringstream ss;
	ss << "NAME                     TYPE      VALUE" << endl;

	map<string,Value>::const_iterator i;
	for(i = table.begin(); i != table.end(); ++i) {
		// Output name
		ss << setfill(' ') << setw(25) << std::left << i->first;

		// Output type
		//nodetype t = i->second->GetType();
		

		/*string st;
		switch(t) {
			case commandstmt: st = "command"; break;
			case conststmt: st = "const"; break;
			case intexpr: st = "int"; break;
			case stringexpr: st = "string"; break;
			default: st = "<invalid>";
		}
		ss << setw(10) << std::left << st;

		// Trim newlines and tabs out of the value
		string val = i->second->ToString("");
		if(t == conststmt)
			val = ((ConstDef*)i->second)->GetValue()->ToString("");

		string::size_type vpos = val.find("\n");
		while(vpos != string::npos) {
			val.erase(vpos,1);
			vpos = val.find("\n");
		}

		ss << val << "\n";*/
	}

	return ss.str();
}

/*
 * Returns a string representation of the jumps table
 */
string SymbolTable::JumpsTable() const
{
	stringstream ss;
	ss << "LABEL                    ADDRESS" << endl;
	map<string,Anchor*>::const_iterator i;
	for(i = jumps.begin(); i != jumps.end(); ++i) {
		ss << setw(25) << std::left << i->first;
		ss << setbase(16) << i->second->GetTarget() << endl;
	}
	return ss.str();
}

/*
 * Returns a const reference to the label table
 */
const map<string, Anchor*>& SymbolTable::GetJumpTable() const
{
	return jumps;
}

