#ifndef __roArray_h__
#define __roArray_h__

#include "roBytePtr.h"
#include "roMemory.h"
#include "roStatus.h"
#include "roTypeOf.h"
#include "roUtility.h"

namespace ro {

template<class T>
T* arrayFind(T* begin, T* end, const T& val);

template<class T>
T* arrayFind(T* begin, roSize count, const T& val);


// ----------------------------------------------------------------------

/// Array class with static size.
/// A replacement for plan old array, the index operator[] has range check in debug mode.
template<class T, roSize N_>
struct StaticArray
{
	enum { N = N_ };
	T data[N];		///< Fixed-size array of elements of type T

	T&				operator[](roSize i)		{ roAssert(i < N); return data[i]; }
	const T&		operator[](roSize i) const	{ roAssert(i < N); return data[i]; }

	T&				front()						{ return data[0]; }
	const T&		front() const				{ return data[0]; }

	T&				back()						{ return data[N-1]; }
	const T&		back() const				{ return data[N-1]; }

	roSize			size() const				{ return N; }
	roSize			byteSize() const			{ return sizeof(data); }

	T*				typedPtr()					{ return data; }
	const T*		typedPtr() const			{ return data; }

	roBytePtr		bytePtr()					{ return data; }
	const roBytePtr	bytePtr() const				{ return data; }

	void			assign(const T& fill);		///< Assign one value to all elements

	roStaticAssert(N_ > 0);
};	// StaticArray


// ----------------------------------------------------------------------

/// Interface for common dynamic array operations
template<class T_, class Super>
struct IArray
{
	typedef T_ T;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;

	IArray() : _size(0), _capacity(0), _data(NULL) {}

// Operations
	bool		isEmpty() const					{ return (_size <= 0); }

	T&			front()							{ roAssert(_size > 0); return _data[0]; }
	const T&	front() const					{ roAssert(_size > 0); return _data[0]; }

	T&			back(roSize i=0)				{ roAssert(_size > 0); return _data[_size - i - 1]; }
	const T&	back(roSize i=0) const			{ roAssert(_size > 0); return _data[_size - i - 1]; }

	T&			operator[](roSize idx)			{ roAssert(idx < _size); return _data[idx]; }
	const T&	operator[](roSize idx) const	{ roAssert(idx < _size); return _data[idx]; }

	void		swap(IArray& rhs)				{ roSwap(_size, rhs._size); roSwap(_capacity, rhs._capacity); roSwap(_data, rhs._data); }

	Status		copy(const Super& src);

	Status		resize(roSize newSize, const T& fill=T());
	Status		incSize(roSize size, const T& fill=T());
	void		clear();
	void		condense();

	T&			pushBack(const T& val=T());
	T&			pushBackBySwap(const T& val);
	T&			insert(roSize idx, const T& val);
	T&			insert(roSize idx, const T* srcBegin, const T* srcEnd);

	void		popBack();
	void		remove(roSize i);
	void		removeBySwap(roSize i);
	void		removeByKey(const T& key);

	T*			find(const T& key);
	const T*	find(const T& key) const;

// Attributes
	iterator		begin()				{ return _data; }
	const_iterator	begin() const		{ return _data; }

	iterator		end()				{ return _data + _size; }
	const_iterator	end() const			{ return _data + _size; }

	roSize			size() const		{ return _size; }
	roSize			sizeInByte() const	{ return _size * sizeof(T); }
	roSize			capacity() const	{ return _capacity; }

	T*				typedPtr()			{ return _data; }
	const T*		typedPtr() const	{ return _data; }

	template<class U>
	U*				castedPtr()			{ return (U*)_data; }
	template<class U>
	const U*		castedPtr() const	{ return (U*)_data; }

	roBytePtr		bytePtr()			{ return _data; }
	const roBytePtr	bytePtr() const		{ return _data; }

// Private
	roSize _size;
	roSize _capacity;
	T* _data;
};	// IArray


// ----------------------------------------------------------------------

/// Dynamic array in it's standard form (similar to std::vector)
template<class T>
struct Array : public IArray<T, Array<T> >
{
	Array() {}
	Array(roSize size)					{ resize(size); }
	Array(roSize size, const T& val)	{ resize(size, val); }
	Array(const Array<T>& src)			{ copy(src); }
	~Array()							{ clear(); roFree(_data); }
	Array&	operator=(const Array& rhs) { copy(rhs); return *this; }

// Operations
	Status reserve(roSize newCapacity);
};	// Array

template<class T, roSize PreAllocCount>
struct TinyArray : public IArray<T, TinyArray<T,PreAllocCount> >
{
	TinyArray()										{ _data = (T*)_buffer; _capacity = PreAllocCount; }
	TinyArray(roSize size)							{ resize(size); }
	TinyArray(roSize size, const T& val)			{ resize(size, val); }
	TinyArray(const TinyArray<T,PreAllocCount>& v)	{ _data = (T*)_buffer; _capacity = PreAllocCount;  copy(v); }
	~TinyArray()									{ clear(); if(_data != (T*)_buffer) roFree(_data); }
	TinyArray& operator=(const TinyArray& rhs)		{ copy(rhs); }

// Operations
	void insert(roSize idx, const T& val);

	Status reserve(roSize newSize);

// Private
	char _buffer[PreAllocCount * sizeof(T)];
};	// TinyArray

typedef Array<roUint8> ByteArray;


// ----------------------------------------------------------------------

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

	T&			operator[](roSize i) const	{ roAssert(i < size); return *(data + i*stride).cast<T>(); }

	T*			typedPtr() const			{ return data.cast<T>(); }
	roBytePtr	bytePtr() const				{ return data; }

	roSize		byteSize() const			{ return size * stride; }

	bool		isEmpty() const				{ return !data || size == 0 || stride == 0; }

	roBytePtr data;
	roSize size;	///< Element count.
	roSize stride;
};	// StrideArray


// ----------------------------------------------------------------------

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

	T&			operator[](roSize i) const	{ roAssert(i < size); return *(data + i*stride_).cast<T>(); }

	T*			typedPtr() const			{ return data.cast<T>(); }
	roBytePtr	bytePtr() const				{ return data; }

	roSize		stride() const				{ return stride_; }
	roSize		byteSize() const			{ return size * stride_; }

	roBytePtr data;
	roSize size;	///< Element count.

#if roDEBUG
	roSize cStride;	///< For Visual Studio debugger visualization purpose.
#endif
};	// ConstStrideArray

}	// namespace ro

#include "roArray.inl"

#endif	// __roArray_h__
