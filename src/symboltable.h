/* symboltable class */
#pragma once

#include <vector>
#include <string>
#include <map>

class Anchor;
struct Value;

class SymbolTable
{
private:
	SymbolTable* parent;		// chaining scopes
	std::map<std::string, Value> table;
	std::map<std::string, Anchor*> jumps;

public:
	explicit SymbolTable(SymbolTable* parent = NULL) {
		this->parent = parent;
	}
	SymbolTable(const SymbolTable&);
	~SymbolTable();

	void Merge(const SymbolTable& other, /*out*/ std::vector<std::string>& collisions);

	void SetParent(SymbolTable* parent);

	// Adds a base address to all registered label targets
	// TODO: labels really should be factored out of this class and into Module
	void AddBaseAddress(unsigned int base);

	bool Define(const std::string& name, Value val);

	bool DefineAnchor(Anchor* anchor);

	bool DefineAnchor(const std::string& name, Anchor* anchor);

	// Symbol lookup
	Value Lookup(const std::string& name) const;
	Anchor* LookupAnchor(const std::string& name) const;

	// Lookup with no parent scope chaining
	Value Get(const std::string& name) const;
	Anchor* GetAnchor(const std::string& name) const;

	std::string ToString() const;

	std::string JumpsTable() const;

	// TODO: this isn't really a sensible interface member
	const std::map<std::string, Anchor*>& GetJumpTable() const;
};

