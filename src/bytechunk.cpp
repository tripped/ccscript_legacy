/* bytechunk implementation */

#include "bytechunk.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include "anchor.h"
#include "ast.h"
#include "exception.h"

using namespace std;

ByteChunk::ByteChunk()
	: pos(0)
{
}

ByteChunk::ByteChunk(const string& str)
	: pos(0)
{
	for(string::const_iterator it = str.begin(); it != str.end(); ++it)
		Char(*it);
}

ByteChunk::ByteChunk(const ByteChunk& other)
	: bytes(other.bytes), /*refs(other.refs),*/ pos(other.pos),
	  baseaddress(other.baseaddress), cinfo(other.cinfo)
{
	// Oof, copy constructor. Of course.
	// We need to copy anchors as well.
	other.TranslateReferences(*this, 0, 0, other.GetSize());
}



bool ByteChunk::operator==(const ByteChunk& rhs) const
{
	// Two strings are equal iff:
	//  - they are of the same length and their bytes are identical, AND
	//  - their lists of references and anchors are identical
	// (The character "pretty-printing" info in the cinfo vector is
	// not considered part of the string for equality testing purposes.)

	if(bytes.size() != rhs.bytes.size())
		return false;
	if(!equal(bytes.begin(), bytes.end(), rhs.bytes.begin()))
		return false;

	if(refs.size() != rhs.refs.size())
		return false;
	if(!equal(refs.begin(), refs.end(), rhs.refs.begin()))
		return false;

	return true;
}

bool ByteChunk::operator!=(const ByteChunk& rhs) const
{
	return !operator==(rhs);
}

bool ByteChunk::operator==(const string& rhs) const
{
	return operator==(ByteChunk(rhs));
}

bool ByteChunk::operator!=(const string& rhs) const
{
	return !operator==(rhs);
}

//
// Friend operators (for using a std::string as the lhs of a comparison)
//
bool operator==(const string& lhs, const ByteChunk& rhs)
{
	return rhs == lhs;
}

bool operator!=(const string& lhs, const ByteChunk& rhs)
{
	return rhs != lhs;
}


//
// String::Reference comparison operators
//
bool ByteChunk::Reference::operator==(const ByteChunk::Reference& rhs) const
{
	return (rhs.location == this->location) &&
		(rhs.offset == this->offset) &&
		(rhs.length == this->length) &&
		(rhs.target == this->target);
}

bool ByteChunk::Reference::operator!=(const ByteChunk::Reference& rhs) const
{
	return !operator==(rhs);
}




unsigned int ByteChunk::GetPos() const
{
	return pos;
}

unsigned int ByteChunk::GetSize() const
{
	return bytes.size();
}

void ByteChunk::Byte(unsigned int n)
{
	pos++;
	bytes.push_back((unsigned char)n);
	cinfo.push_back(false);
}

void ByteChunk::Char(unsigned int n)
{
	// TODO: character set mapping should be moved to a higher level;
	// we want to be able to support multiple mappings easily.
	Byte(n + 0x30);
	cinfo[pos-1] = true;
}

void ByteChunk::Short(unsigned int n)
{
	Byte(n & 255);
	Byte((n >> 8) & 255);
}

void ByteChunk::Long(unsigned int n)
{
	Byte(n & 255);
	Byte((n >> 8) & 255);
	Byte((n >> 16) & 255);
	Byte((n >> 24) & 255);
}

void ByteChunk::Code(const string &code)
{
	unsigned int i = 0;
	char b[3];
	while(i < code.length()) {
		if(code[i] == ' ') { i++; continue; }
		b[0] = code[i++];
		b[1] = code[i++];
		b[2] = 0;
		int n = strtoul(b, NULL, 16);
		Byte(n);
	}
}

void ByteChunk::Truncate(unsigned int newsize)
{
	if(newsize > bytes.size()) return;
	bytes.resize(newsize);

	pos = bytes.size();

	cinfo.resize(newsize);
}


// Appends the contents of another ByteChunk to this
void ByteChunk::Append(const ByteChunk& other)
{
	// First append all references, offsetting their location by current size
	other.TranslateReferences(*this, -(signed)GetSize(), 0, other.GetSize());

	// Then append the actual data
	for(vector<unsigned char>::const_iterator it
		= other.bytes.begin(); it != other.bytes.end(); ++it)
	{
		bytes.push_back(*it);
		++pos;
	}

	// Finally, character mask info
	for(vector<bool>::const_iterator it
		= other.cinfo.begin(); it != other.cinfo.end(); ++it)
	{
		cinfo.push_back(*it);
	}
}


ByteChunk ByteChunk::Substring(unsigned int start, unsigned int len) const
{
	if(start >= bytes.size() || (start + len) > bytes.size())
		throw Exception("substring range out of bounds");

	// First, create an empty string and copy the desired bytes into it:
	ByteChunk substr;

	substr.bytes.resize(len);
	copy(bytes.begin() + start, bytes.begin() + start + len,
		substr.bytes.begin());
	substr.cinfo.resize(len);
	copy(cinfo.begin() + start, cinfo.begin() + start + len,
		substr.cinfo.begin());

	// Copy translated references
	TranslateReferences(substr, start, start, len);

	return substr;
}


//
// Takes all the references in the specified range and copies them into
// the destination string, translating them by the specified offset.
//
void ByteChunk::TranslateReferences(ByteChunk& destination,
	int offset, unsigned int start, unsigned int len) const
{
	// This is not a trivial operation! It is possible for references
	// to be truncated -- if a 4-byte label reference has its first two
	// bytes before the specified start point, we still need to record
	// the fact that the last two bytes of the label's address should be
	// filled in in the remaining space!
	//
	// So, we include every reference that is at least partially in the
	// substring range, and modify the recorded bounds of each one that
	// happened to be truncated.

	// List of references that need to be included in the substring:
	vector<Reference> needed_refs = GetReferencesInRange(start, len);

	// Map of translated anchors:
	typedef map<Anchor*, Anchor*> trans_map;
	trans_map translated;

	//
	// First, ALWAYS transfer "external" anchors.
	//
	for(vector<Anchor*>::const_iterator it = anchors.begin();
		it != anchors.end(); ++it)
	{
		Anchor* a = *it;
		if(a->IsExternal()) {
			destination.AddAnchor( a->GetPosition() - offset, a );
			translated[a] = a;
		}
	}


	for(vector<Reference>::const_iterator it = needed_refs.begin();
		it != needed_refs.end(); ++it)
	{
		Reference r(*it);

		// If this reference refers to a local anchor, we should copy
		// that as well. (Unused non-external anchors are omitted.)

		vector<Anchor*>::const_iterator found =
			find(anchors.begin(), anchors.end(), r.target);

		if(found != anchors.end()) {
			// This means the reference is to a local anchor; we should
			// translate and copy it if it's in range
			Anchor* a = *found;
			if((a->GetPosition() < (signed)start || a->GetPosition() > (signed)(start + len))
				&& !a->IsExternal())
				throw Exception("substring operation truncated necessary anchor: " + a->GetName());

			// Check if this anchor has already been mapped
			trans_map::iterator already_mapped = translated.find( a );

			if(already_mapped == translated.end()) {
				Anchor* ss_anchor = new Anchor(*a);
				destination.AddAnchor( a->GetPosition() - offset, ss_anchor );
				translated[a] = ss_anchor;
				r.target = ss_anchor;
			}
			else
			{
				r.target = already_mapped->second;
			}
		}

		// Now finish translating the reference, accounting for possible truncation

		// the offset of the reference within the specified range
		r.location -= start;

		// the positions of the first and last bytes of the reference
		int refstart = r.location + r.offset;
		int refend = r.location + r.offset + r.length - 1;

		// If part of the reference falls outside the range, we need to adjust its bounds
		if(refstart < 0 || refend >= (signed)len)
		{
			// how many bytes were cut off at the beginning?
			r.offset = -(std::min(r.location, 0));
			// how many bytes were cut off at the end?
			int overflow = std::max(0, refend - (signed)(len - 1));
			// how many total bytes of the reference are left?
			r.length = std::min(r.length, r.length - r.offset - overflow);
		}

		// Add the reference
		destination.AddReference(r.location + start - offset, r.offset, r.length, r.target);
	}
}



void ByteChunk::AddReference(unsigned int location, Anchor *target)
{
	Reference r;
	r.location = location;
	r.target = target;
	r.offset = 0;
	r.length = 4;
	refs.push_back(r);
}

void ByteChunk::AddReference(unsigned int location, int offset, int length, Anchor* target)
{
	Reference r;
	r.location = location;
	r.target = target;
	r.offset = offset;
	r.length = length;
	refs.push_back(r);
}

//
// Places an anchor at the end of the string.
// The string takes ownership of the anchor pointer.
//
void ByteChunk::AddAnchor(Anchor* anchor)
{
	AddAnchor(GetSize(), anchor);
}

//
// Places an anchor at the given position within the string.
// The string takes ownership of the anchor pointer.
//
void ByteChunk::AddAnchor(int pos, Anchor* anchor)
{
	anchor->SetPosition(pos);
	anchors.push_back(anchor);
}

vector<Anchor*> ByteChunk::GetAnchors() const
{
	vector<Anchor*> results;
	results.resize(anchors.size());
	copy(anchors.begin(), anchors.end(), results.begin());
	return results;
}

vector<ByteChunk::Reference> ByteChunk::GetReferences() const
{
	vector<Reference> results;
	results.resize(refs.size());
	copy(refs.begin(), refs.end(), results.begin());
	return results;
}


vector<ByteChunk::Reference> ByteChunk::GetReferencesInRange(unsigned int start, unsigned int size) const
{
	vector<Reference> results;
	if(size == 0)
		return results;

	vector<Reference>::const_iterator it;
	for(it = refs.begin(); it != refs.end(); ++it) {
		// A reference is in the range iff:
		//  - the first byte is before the end of the range
		//  - the last byte is at or after the beginning of the range
		int refstart = it->location + it->offset;
		int refend = it->location + it->offset + it->length - 1;
		if(refstart < (signed)(start + size) && refend >= (signed)start)
			results.push_back(*it);
	}
	return results;
}


//
// Sets a base address for the anchors contained in the string, and updates
// their physical addresses accordingly.
//
void ByteChunk::SetBaseAddress(unsigned int adr)
{
	baseaddress = adr;

	vector<Anchor*>::iterator it;
	for(it = anchors.begin(); it != anchors.end(); ++it)
	{
		(*it)->SetTarget( (*it)->GetPosition() + baseaddress );
	}
}




//
// Writes the final addresses of all label references into the chunk.
// Do not call until all label addresses have been computed. :-)
//
void ByteChunk::ResolveReferences()
{
	try {
		for(unsigned int i = 0; i < refs.size(); ++i) {
			// Get the target address
			unsigned int adr = refs[i].target->GetTarget();
			// Write it to the reference location
			int loc = refs[i].location;
			int offset = refs[i].offset;
			int count = refs[i].length;

			for(int j = offset; j < offset + count; ++j)
				bytes.at(loc + j) = (adr >> (j*8)) & 255;
		}
	}
	catch(...)
	{
		throw Exception("Oopsie");
	}
}

//
// Data reading methods
//
unsigned char ByteChunk::ReadByte(unsigned int pos) const
{
	try {
		return bytes.at(pos);
	}
	catch(std::exception&) {}
	return 0;
}

unsigned short ByteChunk::ReadShort(unsigned int pos) const
{
	unsigned short result = 0;
	try {
		result += bytes.at(pos);
		result += bytes.at(pos+1) << 8;
	}
	catch(std::exception&) {
		// just give incomplete results on exception
	}
	return result;
}

unsigned int ByteChunk::ReadLong(unsigned int pos) const
{
	unsigned long result = 0;
	try {
		result += bytes.at(pos);
		result += bytes.at(pos+1) << 8;
		result += bytes.at(pos+2) << 16;
		result += bytes.at(pos+3) << 24;
	}
	catch(std::exception&) {
	}
	return result;
}

/*
 * Writes the chunk to a specified buffer.
 *
 * NOTE: "location" is assumed to be a virtual address that needs
 * to be mapped to a physical address within the buffer.
 * Returns false if a write was attempted past the end of the buffer.
 */
bool ByteChunk::WriteChunk(char* buffer, int location, int bufsize) const
{
	for(unsigned int i = 0; i < bytes.size(); ++i) {
		int a = location + i;
		if(a >= bufsize)
			return false;
		buffer[a] = bytes[i];
	}
	return true;
}



string ByteChunk::ToString() const
{
	stringstream result;

	for(unsigned int i = 0; i < bytes.size(); ++i) {
		if(!cinfo[i]) {
			if(i == 0 || cinfo[i-1])
				result << '[';
			result << setfill('0') << setw(2) << hex << (int)bytes[i];

			if(i == bytes.size()-1 || cinfo[i+1])
				result << ']';
			else
				result << ' ';
		}
		else
			result << (char)(bytes[i] - 0x30);
	}
	return result.str();
}

ostream& operator<<(ostream& stream, const ByteChunk& rhs)
{
	stream << rhs.ToString();
	return stream;
}

