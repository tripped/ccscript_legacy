/* bytechunk class */
#pragma once

#include <vector>
#include <iomanip>
#include <cstdlib>
#include <cstring>

class Anchor;


// Changes for CCScript 2.0:
//
// Firstly, this class will be renamed to "String" to represent its place
// as one of CCScript 2.0's fundamental types.
//
// Beyond that, its interface will be changed somewhat: it will no longer
// support operations like SetBaseAddress(), which pertain to its now
// outmoded role as a close interactor with the underlying ROM data. It
// will also no longer have an internal "position".
//
// It will still have internal References, and what is more we will also
// add "Anchors," providing relative locations for References to be linked
// to. (This is necessary because in CCScript 2.0 it will often be the case
// that we evaluate a string mutator like "and" that needs internal jump
// codes before we actually know its location in the ROM.)
//
// The first immediate change, though, is the addition of an Append method,
// which allows one ByteChunk to be appended to another.
//
// We'll also need a Substring method, which returns a specified portion of
// the ByteChunk, with anchors and possibly truncated references. (This
// raises a question: what do we do when a substring slices out an anchor
// to which a reference in the remaining portion refers? Hrm. I can't think
// of any situation in which you'd want to be able to do that, and I also
// can't think of any reasonable way to recover from it. I think that should
// be an error case.)


class ByteChunk
{
public:
	// Internal references
	//
	// NOTE: this structure is a little weird. The 'location' field does not
	// necessarily identify the offset in the string where the reference data
	// starts. Rather, it identifies the offset where the reference _would_
	// start if it were "whole" -- if a reference is truncated by having its
	// first two bytes shaved off, its location will remain the same, but its
	// "offset" will become 2.
	//
	// Thus, location+offset is the actual beginning of the reference.
	struct Reference {
		int location;	// relative location of the first byte of the reference
		int offset;	// first byte of reference that will actually be written
		int length; // length; bytes in (offset, offset+length) of target are put at location
		Anchor* target;

		Reference()
			: location(0), offset(0), length(0), target(NULL) { }
		Reference(unsigned int loc, int off, int len, Anchor* t)
			: location(loc), offset(off), length(len), target(t) { }

		bool operator==(const Reference& rhs) const;
		bool operator!=(const Reference& rhs) const;
	};

public:
	ByteChunk();
	ByteChunk(const std::string& str);
	ByteChunk(const ByteChunk&);

	//
	// Relational operators
	//
	bool operator==(const ByteChunk& rhs) const;
	bool operator!=(const ByteChunk& rhs) const;
	// Allow comparisons to std::strings:
	bool operator==(const std::string& rhs) const;
	bool operator!=(const std::string& rhs) const;
	friend bool operator==(const std::string& lhs, const ByteChunk& rhs);
	friend bool operator!=(const std::string& lhs, const ByteChunk& rhs);

	unsigned int GetSize() const;

	// Same thing as GetSize(), actually :I
	// REMOVE
	unsigned int GetPos() const;

	//
	// Reference handling
	//
public:
	void AddReference(unsigned int location, Anchor* target);
	void AddReference(unsigned int location, int offset, int length, Anchor* target);

	void AddAnchor(Anchor* anchor);
	void AddAnchor(int location, Anchor* anchor);
	
	std::vector<Reference> GetReferencesInRange(unsigned int start, unsigned int size) const;
	std::vector<Reference> GetReferences() const;
	std::vector<Anchor*> GetAnchors() const;

	void SetBaseAddress(unsigned int adr);
	void ResolveReferences();

private:
	void TranslateReferences(ByteChunk& destination,
		int offset, unsigned int start, unsigned int len) const;


	//
	// Output primitives
	//
public:
	void Byte(unsigned int n);
	void Char(unsigned int n);
	void Short(unsigned int n);
	void Long(unsigned int n);

	

	// Writes a series of hex bytes to the code chunk from a string.
	// NOTE: string must be a valid sequence of hex digit pairs, optionally separated by spaces.
	// Performs no bounds checking. For internal use only.
	void Code(const std::string &code);


	//
	// String operations
	//

	// Truncates the chunk to the given size
	void Truncate(unsigned int newsize);

	// Appends the contents of another ByteChunk to this one
	void Append(const ByteChunk& other);

	// Returns a substring of this ByteChunk
	ByteChunk Substring(unsigned int start, unsigned int len) const;


	// Some methods for reading data out of a chunk
	unsigned char ReadByte(unsigned int pos) const;
	unsigned short ReadShort(unsigned int pos) const;
	unsigned int ReadLong(unsigned int pos) const;

	// Writes the contents of the chunk to a buffer
	bool WriteChunk(char* buffer, int location, int bufsize) const;


	//
	// String printing
	//
	std::string ToString() const;
	friend std::ostream& operator<<(std::ostream& stream, const ByteChunk& rhs);


	// TODO: replace this with a proper ToString
	void PrintCode() const {
		for(unsigned int i = 0; i < bytes.size(); ++i) {
			if(!cinfo[i]) {
				if(i == 0 || cinfo[i-1]) {
					printf("[");
				}
				printf("%02X", bytes[i]);
				if(i == bytes.size()-1 || cinfo[i+1]) {
					printf("]");
				} else printf(" ");
			}
			else
				printf("%c", bytes[i]-0x30);
		}
	}

private:
	std::vector<unsigned char> bytes;
	std::vector<Reference> refs;
	std::vector<Anchor*> anchors;

	unsigned int pos;
	unsigned int baseaddress;

	// temporary parallel vector for debug printing
	std::vector<bool> cinfo;
};

