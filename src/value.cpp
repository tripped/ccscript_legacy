/* CCScript Value implementation */

#include "value.h"

#include <sstream>
#include <string>

#include "table.h"
#include "function.h"
#include "string.h"

//////////////////////////////////////////////
// Null

Value Value::Null = Value();
Value Value::Undefined = Value(Type::Undefined);


//////////////////////////////////////////////
// Types

// Default constructor: initializes undefined type
Type::Type() : t(Undefined)
{
}

// Copy constructor: copies type from other Type object
Type::Type(const Type& other) : t(other.t)
{
}

// Constructs a type object from one of the Type enumeration constants
Type::Type(Type::EType t)
{
	this->t = t;
}

// Assigns the value of another Type to this Type
Type& Type::operator=(const Type& rhs)
{
	t = rhs.t;
	return *this;
}

// Assigns a Type enumeration constant to this Type
Type& Type::operator=(Type::EType rhs)
{
	t = rhs;
	return *this;
}


// Ccompare Type objects against other Type objects
bool Type::operator==(const Type& rhs) const
{
	return t == rhs.t;
}
bool Type::operator!=(const Type& rhs) const
{
	return !operator==(rhs);
}

// Compare Type objects against type enumeration constants
bool Type::operator==(Type::EType rhs) const
{
	return t == rhs;
}
bool Type::operator!=(Type::EType rhs) const
{
	return !operator==(rhs);
}

// Implicitly convert Type to Type::EType
//  (All this to avoid C++'s enum scope pollution!)
Type::operator EType() const
{
	return t;
}


// "Friend" relational operators for comparing EType to Type
bool operator==(Type::EType lhs, const Type& rhs)
{
	return lhs == rhs.t;
}

bool operator!=(Type::EType lhs, const Type& rhs)
{
	return !operator==(lhs,rhs);
}


std::ostream& operator<<(std::ostream& stream, const Type& obj)
{
	switch(obj.t) {
		case Type::Number: stream << "Type::Number"; break;
		case Type::Label: stream << "Type::Label"; break;
		case Type::String: stream << "Type::String"; break;
		case Type::Function: stream << "Type::Function"; break;
		case Type::Table: stream << "Type::Table"; break;
		default: stream << "Type::Undefined"; break;
	}
	return stream;
}



//////////////////////////////////////////////
// Values


// Default constructor:
//  Initializes an "undefined" value
Value::Value() : type(Type::Null)
{
	Init();
}

Value::Value(int n)
	: type(Type::Number), val(n)
{
	Init();
}

Value::Value(String* s)
	: type(Type::String), val(s)
{
	Init();
}

Value::Value(Function* f)
	: type(Type::Function), val(f)
{
	Init();
}

Value::Value(Table* t)
	: type(Type::Table), val(t)
{
	Init();
}

Value::Value(Label* l)
	: type(Type::Label), val(l)
{
	Init();
}

Value::Value(Node* n)
	: type(Type::Macro), val(n)
{
	Init();
}

Value::Value(Type type)
	: type(type), val(0)
{
	Init();
}

// New value initialization
void Value::Init()
{
	if(IsRefCounted()) {
		refcount = new int(1);
	}
	else
		refcount = NULL;
}



// Copy constructor
//  Creates a value that is a copy of another value. If the value
//  is a reference type, the reference count is shared and incremented.
Value::Value(const Value& other)
	: type(other.type), val(other.val)
{
	if(IsRefCounted()) {
		refcount = other.refcount;
		(*refcount)++;
	}
}

Value::~Value()
{
	// If this value is of a reference type, decrease the reference count;
	// if the reference count drops to zero, delete the referenced object.
	Release();
}



// Assignment operator
//  Assign the given value to this one, sharing references if necessary
Value& Value::operator =(const Value &rhs)
{
	// First, release the old reference, if applicable
	Release();

	type = rhs.type;
	val = rhs.val;
	if(IsRefCounted()) {
		refcount = rhs.refcount;
		(*refcount)++;
	}
	return *this;
}


bool Value::operator==(const Value& rhs)
{
	if(type == Type::Null && rhs.type == Type::Null)
		return true;

	if(type == Type::Undefined && rhs.type == Type::Undefined)
		return true;
	
	if(type != rhs.type)
		return false;

	switch(type) {
	case Type::Number:
		return val.number == rhs.val.number;
	case Type::String:
		return val.string == rhs.val.string;
	case Type::Table:
		return val.table == rhs.val.table;
	case Type::Function:
		return val.function == rhs.val.function;
	case Type::Label:
		return val.label == rhs.val.label;
	case Type::Macro:
		return val.node == rhs.val.node;
	default:
		return false;
	}
}

bool Value::operator!=(const Value& rhs)
{
	return !(operator==(rhs));
}



Type Value::GetType() const
{
	return type;
}


int Value::GetNumber() const
{
	// TODO: ensure that value has correct type, maybe throw exception
	return val.number;
}

Label* Value::GetLabel() const
{
	return val.label;
}

Node* Value::GetNode() const
{
	return val.node;
}

String* Value::GetWeakString() const
{
	return val.string;
}

Table* Value::GetWeakTable() const
{
	return val.table;
}

Function* Value::GetWeakFunction() const
{
	return val.function;
}



// String conversions
String Value::ToString() const
{
	switch(type)
	{
	case Type::Number:
		{
		std::stringstream ss;
		ss << val.number;
		return ss.str();
		}
	case Type::String:
		// Just return a copy of the string
		return *val.string;
	case Type::Table:
		return String("<table>");
	case Type::Function:
		return String("<function>");
	case Type::Label:
		return String("<label>");
	default:
		return String("<invalid type>");
	}
}


String Value::ToCodeString() const
{
	switch(type)
	{
	case Type::Number:
		{
		String s;
		s.Long(val.number);
		return s;
		}
	case Type::String:
		return *val.string;
	case Type::Table:
		return String("<table>");
	case Type::Function:
		return String("<function>");
	case Type::Label:
		return String("<label>");
	default:
		return String("<invalid type>");
	}
}


Value Value::ToStringValue() const
{
	return Value();
}

Value Value::ToCodeStringValue() const
{
	return Value();
}




bool Value::IsRefCounted() const
{
	return type == Type::String ||
		type == Type::Function ||
		type == Type::Table;
}

bool Value::IsValidRef() const
{
	return (refcount != NULL);
}

void Value::Release()
{
	if(IsRefCounted() && IsValidRef()) {
		(*refcount)--;
		if(*refcount < 1)
			Delete();
	}
	refcount = NULL;
}

void Value::Delete()
{
	switch(type) {
		case Type::String:
			delete val.string;
			break;
		case Type::Function:
			delete val.function;
			break;
		case Type::Table:
			delete val.table;
			break;
		default:
			break;
	}
}

