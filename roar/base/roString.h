#ifndef __roString_h__
#define __roString_h__

#include "roUtility.h"
#include "roStringHash.h"
#include "../platform/roCompiler.h"

namespace ro {

///	A light weight replacement for std::string
/// Assume using UTF-8 encoding
struct String
{
	String();
	String(const char* str);
	String(const char* str, roSize count);
	String(const String& str);
	~String();

	String& operator=(const char* str);
	String& operator=(const String& str);

// Operations
	void		resize		(roSize size);
	String&		append		(const char* str, roSize count);
	String&		assign		(const char* str, roSize count);

	void		clear		();
	String&		erase		(roSize offset, roSize count=npos);

	roSize		find		(char c, roSize offset=0) const;
	roSize		find		(const char* str, roSize offset=0) const;
	roSize		rfind		(char c, roSize offset=0) const;
	roSize		rfind		(const char* str, roSize offset=0) const;

	String		substr		(roSize offset=0, roSize count=npos) const;

	String&		operator+=	(const char* str);
	String&		operator+=	(const String& str);

	void		sprintf		(const char* format, ...);
	bool		fromUtf16	(roUint16* src, roSize maxSrcLen);
	bool		toUtf16		(roUint16* dst, roSize& dstLen);

// Attributes
	roSize		size		() const	{ return _length; }
	bool		isEmpty		() const	{ return _length == 0; }

	char*		c_str		()			{ return _cstr; }
	const char*	c_str		() const	{ return _cstr; }

	char& operator[]		(roSize index) { roAssert(index < _length); return _cstr[index]; }
	char operator[]			(roSize index) const { roAssert(index < _length); return _cstr[index]; }

	static const roSize npos = roSize(-1);

// Private
	char* _cstr;
	roSize _length;
};	// String


// ----------------------------------------------------------------------

///	A string class that ensure only one memory block is allocated for each unique string,
///	much like immutable string class in some language like C# and Python.
///	This class is intended for fast string comparison, as a look up key etc.
///
///	Behind the scene, there is a single global hash table managing all instance of FixString.
///	Whenever a FixString is created, it search for any existing string in the table that match
///	with the input string; return that cached string if yes, otherwise the input string will
///	be hashed and copied into the global hash table.
///
///	Every FixString instances are reference counted by the global hash table, once it's reference
///	count become zero the FixString's corresponding entry in the hash table along with the string
///	data will be deleted.
///
///	This class is thread safe regarding the read/write to the global hash table.
struct ConstString
{
	ConstString();

	///	The input string will copied and reference counted.
	/// Null input string will result an empty string.
	ConstString(const char* str);

	/// Construct with an existing string in the global table, indexed with it's hash value.
	/// Make sure there is an other instance of ConstString constructed with a string content first.
	/// An empty string is resulted if the hash value cannot be found.
	explicit ConstString(StringHash hashValue);

	ConstString(const ConstString& rhs);
	~ConstString();

	ConstString& operator=(const char* rhs);
	ConstString& operator=(const ConstString& rhs);
	ConstString& operator=(const StringHash& stringHash);

	const char*		c_str			() const;
	operator		const char*		() const;

	StringHash		hash			() const;
	StringHash		lowerCaseHash	() const;

	unsigned		size			() const;
	bool			isEmpty			() const;

	bool operator==(const StringHash& stringHash) const;
	bool operator==(const ConstString& rhs) const;
	bool operator> (const ConstString& rhs) const;
	bool operator< (const ConstString& rhs) const;

	struct Node;

// Private
	Node* _node;
#if roDEBUG
	const char* _debugStr;	///< For debugger visualize
#endif
};	// ConstString

}	// namespace ro

inline void roSwap(ro::String& lsh, ro::String& rhs)
{
	roSwap(lsh._cstr, rhs._cstr);
	roSwap(lsh._length, rhs._length);
}

inline void roSwap(ro::ConstString& lsh, ro::ConstString& rhs)
{
	roSwap(lsh._node, rhs._node);
#if roDEBUG
	roSwap(lsh._debugStr, rhs._debugStr);
#endif
}

#endif	// __roString_h__
