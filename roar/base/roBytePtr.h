#ifndef __roBytePtr_h__
#define __roBytePtr_h__

#include "../platform/roCompiler.h"

// A compiler can convert any pointer to void*, we also need such
// auto conversion for char* when we are working with raw byte of memory.
struct roBytePtr
{
	roBytePtr(const void* p=NULL) { ptr = (char*)p; }

	operator char*()			{ return ptr; }
	operator const char*() const{ return ptr; }
	operator bool() const		{ return ptr != NULL; }

	template<class T> T* cast()	{ return reinterpret_cast<T*>(ptr); }

	roBytePtr operator++()		{ ptr++; return *this; }
	roBytePtr operator--()		{ ptr--; return *this; }
	roBytePtr operator++(int)	{ roBytePtr o = ptr; ++ptr; return o; }
	roBytePtr operator--(int)	{ roBytePtr o = ptr; --ptr; return o; }

	void operator+=(roSize v)	{ ptr += v; }
	void operator-=(roSize v)	{ ptr -= v; }

	friend roBytePtr operator+(roBytePtr lhs, roSize rhs)		{ return lhs.ptr + rhs; }
	friend roBytePtr operator+(roSize lhs, roBytePtr rhs)		{ return lhs + rhs.ptr; }
	friend roBytePtr operator+(roBytePtr lhs, roPtrDiff rhs)	{ return lhs.ptr + rhs; }
	friend roBytePtr operator+(roPtrDiff lhs, roBytePtr rhs)	{ return lhs + rhs.ptr; }

	char* ptr;
};	// roBytePtr

#endif	// __roBytePtr_h__
