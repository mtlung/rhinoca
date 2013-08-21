#ifndef __roBytePtr_h__
#define __roBytePtr_h__

#include "../platform/roCompiler.h"

/// A compiler can convert any pointer to void*, we also need such
/// auto conversion for char* when we are working with raw byte of memory.
struct roBytePtr
{
	roBytePtr(const void* p=NULL)					{ ptr = (roByte*)p; }

	operator bool() const							{ return ptr != NULL; }

	template<class T> operator T*()					{ return reinterpret_cast<T*>(ptr); }
	template<class T> T* cast()						{ return reinterpret_cast<T*>(ptr); }

	template<class T> T&		ref()				{ return *reinterpret_cast<T*>(ptr); }
	template<class T> const T&	ref() const			{ return *reinterpret_cast<T*>(ptr); }
	template<class T> T&		ref(roSize i)		{ return *(reinterpret_cast<T*>(ptr) + i); }
	template<class T> const T&	ref(roSize i) const	{ return *(reinterpret_cast<T*>(ptr) + i); }

	roByte&		operator[](int i)					{ return ptr[i]; }
	roByte		operator[](int i) const				{ return ptr[i]; }
	roByte&		operator[](roSize i)				{ return ptr[i]; }
	roByte		operator[](roSize i) const			{ return ptr[i]; }

	roBytePtr	operator++()						{ ptr++; return *this; }
	roBytePtr	operator--()						{ ptr--; return *this; }
	roBytePtr	operator++(int)						{ roBytePtr o = ptr; ++ptr; return o; }
	roBytePtr	operator--(int)						{ roBytePtr o = ptr; --ptr; return o; }

	void		operator+=(roSize v)				{ ptr += v; }
	void		operator-=(roSize v)				{ ptr -= v; }

	friend roBytePtr	operator+(roBytePtr lhs, roSize rhs)	{ return lhs.ptr + rhs; }
	friend roBytePtr	operator+(roSize lhs, roBytePtr rhs)	{ return lhs + rhs.ptr; }
	friend roBytePtr	operator+(roBytePtr lhs, roPtrInt rhs)	{ return lhs.ptr + rhs; }
	friend roBytePtr	operator+(roPtrInt lhs, roBytePtr rhs)	{ return lhs + rhs.ptr; }

	friend roPtrInt	operator-(roBytePtr lhs, roBytePtr rhs) { return lhs.ptr - rhs.ptr; }

	friend bool			operator< (roBytePtr lhs, roBytePtr rhs) { return lhs.ptr <  rhs.ptr; }
	friend bool			operator> (roBytePtr lhs, roBytePtr rhs) { return lhs.ptr >  rhs.ptr; }
	friend bool			operator==(roBytePtr lhs, roBytePtr rhs) { return lhs.ptr == rhs.ptr; }
	friend bool			operator!=(roBytePtr lhs, roBytePtr rhs) { return lhs.ptr != rhs.ptr; }
	friend bool			operator<=(roBytePtr lhs, roBytePtr rhs) { return lhs.ptr <= rhs.ptr; }
	friend bool			operator>=(roBytePtr lhs, roBytePtr rhs) { return lhs.ptr >= rhs.ptr; }

	roByte* ptr;
};	// roBytePtr

#endif	// __roBytePtr_h__
