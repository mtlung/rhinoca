#ifndef __roArray_h__
#define __roArray_h__

#include "roAlgorithm.h"
#include "roBytePtr.h"
#include "roMemory.h"
#include "roSafeInteger.h"
#include "roStatus.h"
#include "roTypeOf.h"
#include "roUtility.h"
#include <initializer_list>

namespace ro {

// ----------------------------------------------------------------------

/// Array class with static size.
/// A replacement for plan old array, the index operator[] has range check in debug mode.
template<class T, roSize N_>
struct StaticArray
{
	enum { N = N_ };
	T data[N];		///< Fixed-size array of elements of type T

	StaticArray() = default;

	StaticArray(std::initializer_list<T> initList) {
		roAssert(initList.size() == N);
		for(roSize i=0; i<N; ++i) { data[i] = *(initList.begin() + i); }
	}

	StaticArray& operator=(std::initializer_list<T> initList) {
		roAssert(initList.size() == N);
		for(roSize i=0; i<N; ++i) { data[i] = *(initList.begin() + i); }
		return *this;
	}

	bool			isInRange(int i) const		{ return i >= 0 && roSize(i) < N; }
	bool			isInRange(roSize i) const	{ return i < N; }

	T&				operator[](roSize i)		{ roAssert(i < N); return data[i]; }
	const T&		operator[](roSize i) const	{ roAssert(i < N); return data[i]; }

	T&				front()						{ return data[0]; }
	const T&		front() const				{ return data[0]; }

	T&				back()						{ return data[N-1]; }
	const T&		back() const				{ return data[N-1]; }

	T*				begin()						{ return data; }
	const T*		begin() const				{ return data; }

	T*				end()						{ return data + N; }
	const T*		end() const					{ return data + N; }

	roSize			size() const				{ return N; }
	roSize			byteSize() const			{ return sizeof(data); }

	T*				typedPtr()					{ return data; }
	const T*		typedPtr() const			{ return data; }

	roBytePtr		bytePtr()					{ return data; }
	const roBytePtr	bytePtr() const				{ return data; }

	void			assign(const T& fill);		///< Assign one value to all elements

	T*				find(const T& key);
	const T*		find(const T& key) const;

	template<class K>
	T*				find(const K& key, bool(*equal)(const T&, const K&));
	template<class K>
	const T*		find(const K& key, bool(*equal)(const T&, const K&)) const;

	roStaticAssert(N_ > 0);
};	// StaticArray


// ----------------------------------------------------------------------

/// Interface for common dynamic array operations
template<class T_>
struct IArray
{
	typedef T_ T;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;

	IArray() : _size(0), _capacity(0), _data(NULL) {}
	virtual ~IArray() {}

// Operations
	bool		isEmpty() const					{ return (_size <= 0); }

	bool		isInRange(int i) const			{ return i >= 0 && roSize(i) < size(); }
	bool		isInRange(roSize i) const		{ return i < size(); }
	bool		isInRange(void* p) const		{ return p >= castedPtr<roByte>() && p < (castedPtr<roByte>() + sizeInByte()); }

	void		swap(IArray& rhs)				{ roSwap(_size, rhs._size); roSwap(_capacity, rhs._capacity); roSwap(_data, rhs._data); }

	Status		copy(const IArray& src);
	void		assign(const T& src);
	Status		assign(const T* srcBegin, roSize count);
	Status		assign(const T* srcBegin, const T* srcEnd);

	Status		resize(roSize newSize, const T& fill=T());
	Status		incSize(roSize inc, const T& fill=T());
	Status		decSize(roSize dec);

	Status		resizeNoInit(roSize newSize);	///< For optimization purpose on something like raw byte buffer, use with care
	Status		incSizeNoInit(roSize size);		///< For optimization purpose on something like raw byte buffer, use with care

	void		clear();		///< Has the same effect as resize(0), if you want to clear the memory, use condense
	void		condense();

// Insertion operations
	Status		pushBack(const T& val=T());
	Status		pushBack(const T* srcBegin, roSize count);
	Status		pushBackBySwap(const T& val);
	Status		pushBackUnique(const T& val);
	Status		insert(roSize atIdx, const T& val);
	Status		insert(roSize atIdx, const T* srcBegin, roSize count);
	Status		insert(roSize atIdx, const T* srcBegin, const T* srcEnd);
	Status		insertUnique(roSize atIdx, const T& val);
	Status		insertSorted(const T& val);
	Status		insertSorted(const T& val, bool(*less)(const T&, const T&));

	template<class... Args>
	Status		emplaceBack(Args&&... args);

// Removal operations
	void		popBack();
	void		remove(const T& ele);
	void		removeAt(roSize i);
	void		removeAt(roSize i, roSize count);
	void		removeBySwap(const T& ele);
	void		removeBySwapAt(roSize i);

	template<class K>
	bool		removeByKey(const K& key, bool(*equal)(const T&, const K&) = _defaultEquality);		///< Returns false if the key not found

	template<class K>
	bool		removeAllByKey(const K& key, bool(*equal)(const T&, const K&) = _defaultEquality);

	template<class K>
	void		removeAllByKeys(const IArray<K>& rhs, bool(*equal)(const T&, const K&) = _defaultEquality);	/// All occurrence of rhs in this array will be removed, O(m*n) since unsorted

// Search operations
	T*			find(const T& key);
	const T*	find(const T& key) const;

	template<class K>
	T*			find(const K& key, bool(*equal)(const T&, const K&));
	template<class K>
	const T*	find(const K& key, bool(*equal)(const T&, const K&)) const;

	virtual Status	reserve(roSize newSize, bool force) = 0;

// Attributes
	T&				operator[](roSize idx)		{ roAssert(idx < _size); return _data[idx]; }
	const T&		operator[](roSize idx) const{ roAssert(idx < _size); return _data[idx]; }

	T&				front()						{ roAssert(_size > 0); return _data[0]; }
	const T&		front() const				{ roAssert(_size > 0); return _data[0]; }

	T&				back(roSize i=0)			{ roAssert(_size > 0); return _data[_size - i - 1]; }
	const T&		back(roSize i=0) const		{ roAssert(_size > 0); return _data[_size - i - 1]; }

	iterator		begin()						{ return _data; }
	const_iterator	begin() const				{ return _data; }

	iterator		end()						{ return _data + _size; }
	const_iterator	end() const					{ return _data + _size; }

	roSize			size() const				{ return _size; }
	roSize			sizeInByte() const			{ return _size * sizeof(T); }
	roSize			capacity() const			{ return _capacity; }

	T*				typedPtr()					{ return _data; }
	const T*		typedPtr() const			{ return _data; }

	template<class U>
	U*				castedPtr()					{ return (U*)_data; }
	template<class U>
	const U*		castedPtr() const			{ return (U*)_data; }

	roBytePtr		bytePtr()					{ return _data; }
	const roBytePtr	bytePtr() const				{ return _data; }

// Private
	template<class T1, class T2>
	static bool		_defaultEquality(const T1& lhs, const T2& rhs) { return lhs == rhs; }

	roSize _size;
	roSize _capacity;
	T* _data;
};	// IArray


// ----------------------------------------------------------------------

/// Dynamic array in it's standard form (similar to std::vector)
template<class T>
struct Array : public IArray<T>
{
	Array() {}
	Array(roSize size)					{ this->resize(size); }
	Array(roSize size, const T& val)	{ this->resize(size, val); }
	Array(const Array<T>& src)			{ this->copy(src); }
	Array(Array<T>&& rhs);
	~Array()							{ this->clear(); roFree(this->_data); }

	Array& operator=(const Array& rhs)	{ this->copy(rhs); return *this; }
	Array& operator=(Array&& rhs);

	Array(std::initializer_list<T> initList) { roVerify(assign(initList.begin(), initList.end())); }
	Array& operator=(std::initializer_list<T> initList) { roVerify(assign(initList.begin(), initList.end())); return *this; }

// Operations
	Status reserve(roSize newCapacity, bool force=false) override;
};	// Array

template<class T, roSize PreAllocCount>
struct TinyArray : public IArray<T>
{
	TinyArray()										{ this->_data = (T*)_buffer; this->_capacity = PreAllocCount; }
	TinyArray(roSize size)							{ this->_data = (T*)_buffer; this->_capacity = PreAllocCount; this->resize(size); }
	TinyArray(roSize size, const T& val)			{ this->_data = (T*)_buffer; this->_capacity = PreAllocCount; this->resize(size, val); }
	TinyArray(const TinyArray<T,PreAllocCount>& v)	{ this->_data = (T*)_buffer; this->_capacity = PreAllocCount; copy(v); }
	~TinyArray()									{ this->clear(); if(_isUsingDynamic()) roFree(this->_data); }
	TinyArray& operator=(const TinyArray& rhs)		{ this->copy(rhs); return *this; }

	TinyArray(std::initializer_list<T> initList) { roVerify(assign(initList.begin(), initList.end())); }
	TinyArray& operator=(std::initializer_list<T> initList) { roVerify(assign(initList.begin(), initList.end())); return *this; }

// Operations
	Status reserve(roSize newSize, bool force=false) override;

// Private
	bool _isUsingDynamic() const { return this->_capacity > PreAllocCount; }
	char _buffer[PreAllocCount * sizeof(T)];	/// Don't use array of T to prevent unnecessary constructor call
};	// TinyArray

template<class T>
struct ExtArray : public IArray<T>
{
	ExtArray(void* extBuf, roSize extBufBytes)		{ this->_data = extBuf; this->_capacity = extBufBytes / sizeof(T); }
	~ExtArray()										{ this->clear(); }

// Operations
	Status reserve(roSize newCapacity, bool force=false);
};	// ExtArray

typedef IArray<roByte> IByteArray;
typedef Array<roByte> ByteArray;
typedef TinyArray<roByte, 64> TinyByteArray;

/// Array with a hard limit on it's size, suitable for use in security context to prevent attack
template<class InnerArray>
struct SizeLimitedArray : public InnerArray
{
	SizeLimitedArray() { maxSizeAllowed = 0; }
	SizeLimitedArray(const SizeLimitedArray& src) = delete;
	SizeLimitedArray& operator=(const SizeLimitedArray& rhs) = delete;

// Operations
	Status reserve(roSize newCapacity, bool force=false) override;

	roSize maxSizeAllowed;
};	// SizeLimitedArray


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

	T&			operator[](roSize i) const	{ roAssert(i < size); return *(data + i*stride).template cast<T>(); }

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

	T&			operator[](roSize i) const	{ roAssert(i < size); return *(data + i*stride_).template cast<T>(); }

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

// Stack adapter for IArray
template<class IAry>
struct Stack : public IAry
{
	typedef typename IAry::T T;

	Status		push(const T& val=T())		{ return IAry::pushBack(val); }
	Status		pushUnique(const T& val)	{ return IAry::pushUnique(val); }
	void		pop()						{ IAry::popBack(); }
	T&			top()						{ return IAry::back(); }
	const T&	top() const					{ return IAry::back(); }

protected:
	// Hide selected functions
	Status		pushBack(const T& val=T());
	Status		pushBack(const T* srcBegin, roSize count);
	Status		pushBackBySwap(const T& val);
	Status		insert(roSize atIdx, const T& val);
	Status		insert(roSize atIdx, const T* srcBegin, roSize count);
	Status		insert(roSize atIdx, const T* srcBegin, const T* srcEnd);
	Status		insertUnique(roSize atIdx, const T& val);
	Status		insertSorted(const T& val);
	Status		insertSorted(const T& val, bool(*less)(const T&, const T&));

	void		popBack();
	void		remove(const T& ele);
	void		removeAt(roSize i);
	void		removeAt(roSize i, roSize count);
	void		removeBySwap(const T& ele);
	void		removeBySwapAt(roSize i);
};	// Stack

}	// namespace ro

#endif	// __roArray_h__

#include "roArray.inl"
