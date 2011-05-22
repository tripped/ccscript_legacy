/* CCScript Anchors */
#pragma once

#include <string>

// Anchor is a new class designed to replace the use of "Label" outside
// of the AST classes.
// Basically, an Anchor is a named position within the ROM -- however,
// the exact position of an Anchor might not be known until after a certain
// point in program evaluation. Thus, anchors are resolved when possible,
// and references to anchors are resolved after all code has been evaluated.
// 
// Some anchors are registered by name in symbol tables so that they can
// be referred to by name by other expressions.

class Anchor
{
public:
	Anchor();
	Anchor(const Anchor&);
	explicit Anchor(const std::string& name);
	Anchor(const std::string& name, int pos);

	void SetExternal(bool e);
	void SetPosition(int pos);
	void SetTarget(unsigned int address);

	bool IsExternal() const;
	int GetPosition() const;
	unsigned int GetTarget() const;
	std::string GetName() const;

private:
	std::string name;
	int position;			// anchor position within string
	unsigned int address;	// absolute final address
	bool external;			// TRUE if the anchor is referred to in a symbol table
};

