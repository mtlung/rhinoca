#ifndef __roArray_h__
#define __roArray_h__

#include "roBytePtr.h"
#include "roTypeOf.h"
#include "roUtility.h"

#include <malloc.h>

namespace ro {

/// Generic functions for operate on different kind of array classes

template<class A> inline
void arrayResize(A& ary, roSize newSize);

template<class A> inline
void arrayResize(A& ary, roSize newSize, const typename A::T& fill);

template<class A> inline
void arrayAppend(A& ary, const typename A::T& val);

template<class A> inline
void arrayInsert(A& ary, roSize idx, const typename A::T& val);

template<class A> inline
void arrayRemove(A& ary, roSize idx);

template<class A> inline
void arrayRemoveBySwap(A& ary, roSize idx);

template<class T> inline
T* arrayFind(T* begin, roSize count, const T& val);

template<class T> inline
T* arrayFind(T* begin, T* end, const T& val);

/// Array class with static size.
/// A replacement for plan old array, the index operator[] has range check in debug mode.
template<class T, roSize N_>
struct StaticArray
{
	enum { N = N_ };
	T data[N];	///< Fixed-size array of elements of type T

	T& operator[](roSize i)				{ roAssert(i < N); return data[i]; }
	const T& operator[](roSize i) const	{ roAssert(i < N); return data[i]; }

	T& front()				{ return data[0]; }
	const T& front() const	{ return data[0]; }

	T& back()				{ return data[N-1]; }
	const T& back() const	{ return data[N-1]; }

	roSize size() const		{ return N; }
	roSize byteSize() const	{ return sizeof(data); }

	T* typedPtr()					{ return data; }
	const T* typedPtr() const		{ return data; }

	roBytePtr bytePtr()				{ return data; }
	const roBytePtr bytePtr() const	{ return data; }

	/// Assign one value to all elements
	void assign(const T& value) {
		for(roSize i=0; i<size(); ++i)
			data[i] = value;
	}

	roStaticAssert(N_ > 0);
};	// StaticArray

/// Interface for common dynamic array operations
template<class T_, class Super>
class IArray
{
public:
	typedef T_ T;
	typedef T* iterator;
	typedef const T* const_iterator;

	IArray() : _size(0), _capacity(0), _data(NULL) {}

// Operations
	bool isEmpty() const	{ return (_size <= 0); }

	T& front()				{ roAssert(_size > 0); return _data[0]; }
	const T& front() const	{ roAssert(_size > 0); return _data[0]; }

	T& back(roSize i=0)				{ roAssert(_size > 0); return _data[_size - i - 1]; }
	const T& back(roSize i=0) const	{ roAssert(_size > 0); return _data[_size - i - 1]; }

	T& operator[](roSize idx)				{ roAssert(idx < _size); return _data[idx]; }
	const T& operator[](roSize idx) const	{ roAssert(idx < _size); return _data[idx]; }

	void popBack()					{ roAssert(_size > 0); _size--; _data[_size].~T(); }

	void swap(IArray& rhs)			{ roSwap(_size, rhs._size); roSwap(_capacity, rhs._capacity); roSwap(_data, rhs._data); }

	void resize(roSize newSize)					{ arrayResize(static_cast<Super&>(*this), newSize); }
	void resize(roSize newSize, const T& fill)	{ arrayResize(static_cast<Super&>(*this), newSize, fill); }

	T& pushBack(const T& val)		{ arrayAppend(static_cast<Super&>(*this), val); return back(); }

	void remove(roSize i)			{ arrayRemove(static_cast<Super&>(*this), i); }
	void removeBySwap(roSize i)		{ arrayRemoveBySwap(static_cast<Super&>(*this), i); }

// Attributes
	iterator begin()				{ return _data; }
	const_iterator begin() const	{ return _data; }

	iterator end()					{ return _data + _size; }
	const_iterator end() const		{ return _data + _size; }

	roSize size() const				{ return _size; }
	roSize capacity() const			{ return _capacity; }

	T* typedPtr()					{ return _data; }
	const T* typedPtr() const		{ return _data; }

	roBytePtr bytePtr()				{ return _data; }
	const roBytePtr bytePtr() const	{ return _data; }

	roSize _size;
	roSize _capacity;
	T* _data;
};	// IArray

/// Dynamic array in it's standard form (similar to std::vector)
template<class T>
class Array : public IArray<T, Array<T> >
{
public:
	Array() {}
	inline Array(const Array<T>& v);

	inline ~Array() { arrayResize(*this, 0, T()); free(_data); }

	inline Array& operator=(const Array& rhs);

// Operations
	inline void insert(roSize idx, const T& val);

	void setCapacity(roSize newSize)
	{
		newSize = roMaxOf2(newSize, size());
		_data = (T*)realloc(_data, newSize * sizeof(T));	// NOTE: Here we make the assumption that the object is bit-wise movable
		_capacity = newSize;
	}
};	// Array

template<class T, roSize PreAllocCount>
class TinyArray : public IArray<T, TinyArray<T,PreAllocCount> >
{
public:
	TinyArray() {}
//	inline TinyArray(const TinyArray<T>& v);

	inline ~TinyArray() { arrayResize(*this, 0, T()); free(_data); }

	inline TinyArray& operator=(const TinyArray& rhs);

// Operations
	inline void insert(roSize idx, const T& val);

	void setCapacity(roSize newSize)
	{
		newSize = roMaxOf2(newSize, size());
		_data = (T*)realloc(_data, newSize * sizeof(T));	// NOTE: Here we make the assumption that the object is bit-wise movable
		_capacity = newSize;
	}
};	// TinyArray

/// An class that wrap around a raw pointer and tread it as an array of type T.
/// This class also support array stride, making it very useful when working with
/// vertex buffer.
template<class T>
class StrideArray
{
public:
	StrideArray(const T* data, roSize elementCount, roSize _stride=0)
		: data(data), size(elementCount), stride(_stride == 0 ? sizeof(T) : _stride)
	{
		roAssert(stride >= sizeof(T));
	}

	/// Construct from non-const version of StrideArray<T>, U must have the const qualifier.
	template<class U>
	StrideArray(const StrideArray<U>& rhs)
		: data(rhs.data), size(rhs.size), stride(rhs.stride)
	{}

	T& operator[](roSize i) const
	{
		roAssert(i < size);
		return *(data + i*stride).cast<T>();
	}

	T* typedPtr() const			{ return data.cast<T>(); }
	roBytePtr bytePtr() const	{ return data; }

	roSize byteSize() const		{ return size * stride; }

	bool isEmpty() const		{ return !data || size == 0 || stride == 0; }

	roBytePtr data;
	roSize size;	///< Element count.
	roSize stride;
};	// StrideArray

///	Specialization of StrideArray which give more room for the compiler to do optimization.
template<class T, roSize stride_=sizeof(T)>
class ConstStrideArray
{
public:
	ConstStrideArray(const T* data, roSize elementCount)
		: data(data), size(elementCount)
#if roDEBUG
		, cStride(stride_)
#endif
	{}

	/// Construct from non-const version of ConstStrideArray<T>, U must have the const qualifier.
	template<class U>
	ConstStrideArray(const ConstStrideArray<U>& rhs)
		: data(rhs.data), size(rhs.size)
#if roDEBUG
		, cStride(rhs.stride())
#endif
	{}

	T& operator[](roSize i) const
	{
		roAssert(i < size);
		return *(data + i*stride_).cast<T>();
	}

	T* typedPtr() const { return data.cast<T>(); }
	roBytePtr bytePtr() const { return data; }

	roSize stride() const { return stride_; }
	roSize byteSize() const { return size * stride_; }

	roBytePtr data;
	roSize size;	///< Element count.

#if roDEBUG
	roSize cStride;	///< For Visual Studio debugger visualization purpose.
#endif
};	// ConstStrideArray

}	// namespace ro

#include "roArray.inl"

#endif	// __roArray_h__
