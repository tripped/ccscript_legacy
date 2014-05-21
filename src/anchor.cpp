/* CCScript Anchors */

#include "anchor.h"


Anchor::Anchor()
	: name(""), position(0), address(0), external(false)
{
}

Anchor::Anchor(const Anchor& other)
	: name(other.name), position(other.position), address(other.address),
	  external(other.external)
{
}

Anchor::Anchor(const std::string& name)
	: name(name), position(0), address(0), external(false)
{
}

Anchor::Anchor(const std::string &name, int position)
	: name(name), position(position), address(0), external(false)
{
}

bool Anchor::IsExternal() const
{
	return this->external;
}

int Anchor::GetPosition() const
{
	return this->position;
}

void Anchor::SetExternal(bool e)
{
	this->external = e;
}

void Anchor::SetPosition(int pos)
{
	this->position = pos;
}

void Anchor::SetTarget(unsigned int address)
{
	this->address = address;
}

unsigned int Anchor::GetTarget() const
{
	return this->address;
}

std::string Anchor::GetName() const
{
	return this->name;
}

