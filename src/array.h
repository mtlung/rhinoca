#ifndef __ARRAY_H__
#define __ARRAY_H__

#include "rhassert.h"

template<typename T>
T* rhArrayFind(T* begin, unsigned count, const T& val)
{
	for(unsigned i=0; i<count; ++i)
		if(begin[i] == val)
			return begin + i;
	return NULL;
}

/*! Array class with checked limits.
	A replacement for plan old array, adapted from boost's array.hpp
	The index operator[] has range check in debug mode.
 */
template<typename T, rhuint N_>
class Array
{
public:
	enum { N = N_ };
	T elems[N];	//! Fixed-size array of elements of type T

	T& operator[](rhuint i)
	{
		RHASSERT(i < N);
		return elems[i];
	}

	const T& operator[](rhuint i) const
	{
		RHASSERT(i < N);
		return elems[i];
	}

	//! Element access without range check, faster but use with care.
	T& unsafeGetAt(rhuint i) {
		RHASSERT(i < N);
		return elems[i];
	}

	const T& unsafeGetAt(rhuint i) const {
		RHASSERT(i < N);
		return elems[i];
	}

	T& front() {
		return elems[0];
	}

	const T& front() const {
		return elems[0];
	}

	T& back() {
		return elems[N-1];
	}

	const T& back() const {
		return elems[N-1];
	}

	rhuint size() const {
		return N;
	}

	//! Direct access to data
	T* data() {
		return elems;
	}

	const T* data() const {
		return elems;
	}

	//! Assign one value to all elements
	void assign(const T& value)
	{
		for(rhuint i=0; i<size(); ++i)
			elems[i] = value;
	}
};	// Array

/*!	An class that wrap around a raw pointer and tread it as an array of type T.
	This class also support array stride, making it very usefull when working with
	vertex buffer.
 */
template<typename T>
class StrideArray
{
public:
	StrideArray(const T* _data, rhuint elementCount, rhuint _stride=0)
		: data((char*)_data), size(elementCount), stride(_stride == 0 ? sizeof(T) : _stride)
	{
		RHASSERT(stride >= sizeof(T));
	}

	//! Construct from non-const version of StrideArray<T>, U must have the const qualifier.
	template<typename U>
	StrideArray(const StrideArray<U>& rhs)
		: data((char*)const_cast<T*>(rhs.getPtr())), size(rhs.size), stride(rhs.stride)
	{}

	T& operator[](rhuint i) const
	{
		RHASSERT(i < size);
		return *reinterpret_cast<T*>(data + i*stride);
	}

	T* getPtr() const { return reinterpret_cast<T*>(data); }

	rhuint sizeInByte() const { return size * stride; }

	bool isEmpty() const { return !data || size == 0 || stride == 0; }

	char* data;
	rhuint size;	//!< Element count.
	rhuint stride;
};	// StrideArray

//!	Specialization of StrideArray which give more room for the compiler to do optimization.
template<typename T, rhuint stride_=sizeof(T)>
class FixStrideArray
{
public:
	FixStrideArray(const T* _data, rhuint elementCount)
		: data((char*)_data), size(elementCount)
#ifndef NDEBUG
		, cStride(stride_)
#endif
	{}

	//! Construct from non-const version of FixStrideArray<T>, U must have the const qualifier.
	template<typename U>
	FixStrideArray(const FixStrideArray<U>& rhs)
		: data((char*)const_cast<T*>(rhs.getPtr())), size(rhs.size)
#ifndef NDEBUG
		, cStride(rhs.stride())
#endif
	{}

	T& operator[](rhuint i) const
	{
		RHASSERT(i < size);
		return *reinterpret_cast<T*>(data + i*stride_);
	}

	T* getPtr() const { return reinterpret_cast<T*>(data); }

	rhuint stride() const { return stride_; }
	rhuint sizeInByte() const { return size * stride_; }

	char* data;
	rhuint size;	//!< Element count.

#ifndef NDEBUG
	//! For Visual Studio debugger visualization purpose.
	rhuint cStride;
#endif
};	// FixStrideArray

#endif	// __ARRAY_H__
