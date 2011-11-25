#ifndef __roString_h__
#define __roString_h__

#include "roUtility.h"
#include "../platform/roCompiler.h"

namespace ro {

/// A light weight replacement for std::string
class String
{
public:
	String();
	String(const char* str);
	String(const char* str, roSize count);
	String(const String& str);
	~String();

	String& operator=(const char* str);
	String& operator=(const String& str);

// Operations:
	void resize(roSize size);

	String& append(const char* str, roSize count);

	String& assign(const char* str, roSize count);

	void clear();
	String& erase(roSize offset, roSize count=npos);

	roSize find(char c, roSize offset=0) const;
	roSize find(const char* str, roSize offset=0) const;

	roSize rfind(char c, roSize offset=0) const;
	roSize rfind(const char* str, roSize offset=0) const;

	String substr(roSize offset=0, roSize count=npos) const;

	String& operator+=(const char* str);
	String& operator+=(const String& str);

	void swap(String& rhs);

// Attributes:
	roSize size() const { return _length; }
	bool empty() const { return _length == 0; }

	char* c_str() { return _cstr; }
	const char* c_str() const { return _cstr; }

	char& operator[](roSize index) { roAssert(index < _length); return _cstr[index]; }
	char operator[](roSize index) const { roAssert(index < _length); return _cstr[index]; }

	static const roSize npos = roSize(-1);

// Private:
	char* _cstr;
	roSize _length;
};	// String

}	// namespace ro

inline void roSwap(ro::String& lsh, ro::String& rhs)
{
	roSwap(lsh._cstr, rhs._cstr);
	roSwap(lsh._length, rhs._length);
}

#endif	// __roString_h__
