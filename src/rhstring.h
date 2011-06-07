#ifndef __RHSTRING_H__
#define __RHSTRING_H__

#include "common.h"
#include "rhinoca.h"

// A light weight replacement for std::string
class String
{
public:
	String();
	String(const char* str);
	String(const char* str, unsigned count);
	String(const String& str);
	~String();

	String& operator=(const char* str);
	String& operator=(const String& str);

	typedef unsigned size_type;

// Operations
	void resize(unsigned size);

	String& append(const char* str, size_type count);

	void clear();
	String& erase(size_type offset, size_type count=npos);

	size_type find(char c, size_type offset=0) const;
	size_type find(const char* str, size_type offset=0) const;

	size_type rfind(char c, size_type offset=0) const;
	size_type rfind(const char* str, size_type offset=0) const;

	String substr(size_type offset=0, size_type count=npos) const;

	String& operator+=(const char* str);
	String& operator+=(const String& str);

	void swap(String& rhs);

// Attributes
	size_type size() const { return _length; }
	bool empty() const { return _length == 0; }

	char* c_str() { return _cstr; }
	const char* c_str() const { return _cstr; }

	char& operator[](unsigned index) { ASSERT(index < _length); return _cstr[index]; }
	char operator[](unsigned index) const { ASSERT(index < _length); return _cstr[index]; }

	static const size_type npos = size_type(-1);

protected:
	char* _cstr;
	unsigned _length;
};	// String

/// Reverse version of strstr
char* rstrstr(char* str1, const char* str2);

/*!	Replace certain characters in a string with a pre-defined set of string.
	One example is the encoding of URL:
	const char* uri = "http://www.abc.com/abc 123&456.htm"
	const char* encode[] = { "%20", "%22", "%23", "%24", "%25", "%26", "%3C", "%3E" };
	uri = replaceCharacterWithStr(uri, " \"#$%&<>", encode);
 */
char* replaceCharacterWithStr(const char* str, const char charArray[], const char** replacements);

class StringHash;

/*!	A string class that ensure only one memory block is allocated for each unique string,
	much like immutable string class in some language like C# and Python.
	This class is intended for fast string comparison, as a look up key etc.

	Behind the scene, there is a single global hash table managing all instance of FixString.
	Whenever a FixString is created, it search for any existing string in the table that match
	with the input string; return that cached string if yes, otherwise the input string will
	be hashed and copyed into the global hash table.

	Every FixString instances are reference counted by the global hash table, once it's reference
	count become zero the FixString's corresponding entry in the hash table along with the string
	data will be deleted.

	This class is thread safe regarding the read/write to the global hash table.
 */
class FixString
{
public:
	FixString();

	/*!	The input string will copyed and reference counted.
		Null input string will result an empty string.
	 */
	FixString(const char* str);

	/*!	Construct with an existing string in the global table, indexed with it's hash value.
		Make sure there is an other instance of FixString constructed with a string content first.
		An empty string is resulted if the hash value cannot be found.
	 */
	explicit FixString(rhuint32 hashValue);

	FixString(const FixString& rhs);
	~FixString();

	FixString& operator=(const char* rhs);
	FixString& operator=(const FixString& rhs);
	FixString& operator=(const StringHash& stringHash);

	const char* c_str() const;
	operator const char*() const {	return c_str();	}

	rhuint32 hashValue() const;

	size_t size() const;

	bool empty() const;

	FixString upperCase() const;
	FixString lowerCase() const;

	bool operator==(const StringHash& stringHash) const;
	bool operator==(const FixString& rhs) const;
	bool operator> (const FixString& rhs) const;
	bool operator< (const FixString& rhs) const;

	struct Node;

protected:
	Node* mNode;
#ifndef NDEBUG
	const char* debugStr;	///< For debugger visualize
#endif
};	// FixString

namespace StringHashDetail
{
	// A small utility for defining the "const char (&Type)[N]" argument
	template<int N>
	struct Str { typedef const char (&Type)[N]; };

	template<int N> FORCE_INLINE rhuint32 sdbm(typename Str<N>::Type str)
	{
		// hash(i) = hash(i - 1) * 65599 + str[i]
		// Reference: http://www.cse.yorku.ca/~oz/hash.html sdbm
		return sdbm<N-1>((typename Str<N-1>::Type)str) * 65599 + str[N-2];
	}

	// "1" = char[2]
	// NOTE: Template specialization have to be preformed in namespace scope.
	template<> FORCE_INLINE rhuint32 sdbm<2>(Str<2>::Type str) {
		return str[0];
	}
}	// StringHashDetail

/*!	A simple string hash class.
	Reference http://www.humus.name/index.php?page=Comments&ID=296
 */
class StringHash
{
protected:
	StringHash() {}	// Accessed by derived-classes only

public:
	rhuint32 hash;

	StringHash(rhuint32 h) : hash(h) {}

	StringHash(const FixString& fixString) : hash(fixString.hashValue()) {}

	/*!	The overloaded constructors allow hashing a string literal without run-time cost (in release mode)
		"1" is equals to char[2] since the '\0' is included.
	 */
	FORCE_INLINE StringHash(const char (&str)[2])  {	hash = StringHashDetail::sdbm<2>(str);	}
	FORCE_INLINE StringHash(const char (&str)[3])  {	hash = StringHashDetail::sdbm<3>(str);	}
	FORCE_INLINE StringHash(const char (&str)[4])  {	hash = StringHashDetail::sdbm<4>(str);	}
	FORCE_INLINE StringHash(const char (&str)[5])  {	hash = StringHashDetail::sdbm<5>(str);	}
	FORCE_INLINE StringHash(const char (&str)[6])  {	hash = StringHashDetail::sdbm<6>(str);	}
	FORCE_INLINE StringHash(const char (&str)[7])  {	hash = StringHashDetail::sdbm<7>(str);	}
	FORCE_INLINE StringHash(const char (&str)[8])  {	hash = StringHashDetail::sdbm<8>(str);	}
	FORCE_INLINE StringHash(const char (&str)[9])  {	hash = StringHashDetail::sdbm<9>(str);	}
	FORCE_INLINE StringHash(const char (&str)[10]) {	hash = StringHashDetail::sdbm<10>(str);	}
	FORCE_INLINE StringHash(const char (&str)[11]) {	hash = StringHashDetail::sdbm<11>(str);	}
	FORCE_INLINE StringHash(const char (&str)[12]) {	hash = StringHashDetail::sdbm<12>(str);	}
	FORCE_INLINE StringHash(const char (&str)[13]) {	hash = StringHashDetail::sdbm<13>(str);	}
	FORCE_INLINE StringHash(const char (&str)[14]) {	hash = StringHashDetail::sdbm<14>(str);	}
	FORCE_INLINE StringHash(const char (&str)[15]) {	hash = StringHashDetail::sdbm<15>(str);	}
	FORCE_INLINE StringHash(const char (&str)[16]) {	hash = StringHashDetail::sdbm<16>(str);	}
	FORCE_INLINE StringHash(const char (&str)[17]) {	hash = StringHashDetail::sdbm<17>(str);	}
	FORCE_INLINE StringHash(const char (&str)[18]) {	hash = StringHashDetail::sdbm<18>(str);	}
	FORCE_INLINE StringHash(const char (&str)[19]) {	hash = StringHashDetail::sdbm<19>(str);	}
	FORCE_INLINE StringHash(const char (&str)[20]) {	hash = StringHashDetail::sdbm<20>(str);	}
	FORCE_INLINE StringHash(const char (&str)[21]) {	hash = StringHashDetail::sdbm<21>(str);	}

	//!	Parameter \em len define the maximum length of the input string buf, 0 for auto length detection.
	StringHash(const char* buf, size_t len);
	StringHash(const wchar_t* buf, size_t len);

	operator rhuint32() const {	return hash;	}

	bool operator==(const StringHash& rhs) const	{	return hash == rhs.hash;	}
	bool operator> (const StringHash& rhs) const	{	return hash > rhs.hash;		}
	bool operator< (const StringHash& rhs) const	{	return hash < rhs.hash;		}
};	// StringHash

#endif	// __RHSTRING_H__
