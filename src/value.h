/* CCScript values */
#pragma once

#include <ostream>

#define String ByteChunk

// Forward declarations
class Table;
class Function;
class String;
class Label;
class Node;



// This class defines a "Type" in CCScript. Mainly it's just
// a wrapper around an enum, so we can get around C++'s odd
// habit of polluting enum scopes with the internal enumeration
// members.
struct Type
{
	enum EType {
		Null,
		Number,
		String,
		Function,
		Table,
		Label,
		Macro,	// TEMP; points to an AST node
		Undefined
	};

	// Construction and assignment
	Type();
	Type(EType t);
	Type(const Type& other);
	Type& operator=(const Type& rhs);
	Type& operator=(EType rhs);

	// Comparison
	bool operator==(const Type& rhs) const;
	bool operator!=(const Type& rhs) const;
	bool operator==(EType rhs) const;
	bool operator!=(EType rhs) const;
	friend bool operator==(EType lhs, const Type& rhs);
	friend bool operator!=(EType lhs, const Type& rhs);

	// Implicit conversion
	operator EType() const;

	// Stream output (mainly for testing)
	friend std::ostream& operator<<(std::ostream& stream, const Type& obj);

private:
	EType t;
};

bool operator==(Type::EType lhs, const Type& rhs);
bool operator!=(Type::EType lhs, const Type& rhs);
std::ostream& operator<<(std::ostream& stream, const Type& obj);



// This class defines a "value" in CCScript.
//
// A value can be one of the following:
// - a number
// - a string (by reference)
// - a function (by reference)
// - a table (by reference)
// - a label reference
//
// The first four types are straightforward; the last is a special type
// that is essentially an integer whose value is unknown until the final
// pass of compilation, and is thus somewhat restricted in its use.
//
// A value contains its type, and the actual value. Values can be returned
// from expressions and assigned to variables. Values can also be output
// into the ROM by explicit or implicit output statements.
//
// Certain types of Values are reference-counted; currently this includes
// Table, Function, and String values.
//
struct Value
{
public:
	// Value constructors
	Value();
	Value(int number);
	Value(String* string);
	Value(Function* function);
	Value(Table* table);
	Value(Label* label);
	Value(Node* node);
	Value(Type type);

	virtual ~Value();

	// Copy constructor
	Value(const Value& other);

	// Assignment
	Value& operator=(const Value& rhs);

	// Comparison
	bool operator==(const Value& rhs);
	bool operator!=(const Value& rhs);

	// Properties and methods
	bool IsRefCounted() const;		// Returns true iff this value is of a ref-counted type
	bool IsValidRef() const;		// Returns true iff this value holds a valid reference
	void Release();					// Release reference to ref-counted type

	// Returns the value's type
	Type GetType() const;

	// Value getters
	int GetNumber() const;
	Label* GetLabel() const;
	Node* GetNode() const;

	// These methods are named GetWeak* to indicate that the returned pointer
	// does not protect the underlying object from garbage collection and thus
	// should not be stored for long-term use. If a strong reference preserving
	// the object lifetime is desired, store a copy of the Value object.
	String* GetWeakString() const;
	Table* GetWeakTable() const;
	Function* GetWeakFunction() const;

	// Universal conversions
	String ToString() const;
	String ToCodeString() const;
	Value ToStringValue() const;
	Value ToCodeStringValue() const;

public:
	static Value Null;
	static Value Undefined;

private:
	void Init();

	Type type;
	union UVal {
		int number;
		String* string;
		Table* table;
		Function* function;
		Label* label;
		Node* node;

		UVal() { }
		UVal(int n) { number = n; }
		UVal(String* s) { string = s; }
		UVal(Table* t) { table = t; }
		UVal(Function* f) { function = f; }
		UVal(Label* l) { label = l; }
		UVal(Node* n) { node = n; }
	} val;
	int* refcount;	// for strings, functions, tables
	void Delete();	// deletes ref-counted pointer

	// Note on the "type-value" union and selective deletion, etc.:

	// The fact that we seem to be collecting the data for disparate specializations
	// of a concept in one class, and subsequently worrying about which specialization
	// to use, should suggest a rather obvious solution: make Value a polymorphic
	// base class and let dynamic dispatch do the work for us!

	// The reason we don't do this is that Value is supposed to be a simple, lightweight
	// class that can be easily copied and passed by value, and we couldn't do this if
	// it were polymorphic. The relative bother of having a couple of switch statements
	// to decide which object we're using for a few simple operations is not really a
	// big deal anyway.
};

