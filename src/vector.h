#ifndef __VECTOR_H__
#define __VECTOR_H__

#include "rhassert.h"
#include "common.h"

#ifndef _NEW_
// Define our own placement new and delete operator such that we need not to include <new>
//#include <new>
inline void* operator new(size_t, void* where) { return where; }
inline void operator delete(void*, void*) {}
#endif	// _NEW_

// Mini vector class, supports objects by value
template<typename T> class Vector
{
public:
	typedef T* iterator;
	typedef const T* const_iterator;

	Vector()
		: _vals(NULL), _size(0), _allocated(0)
	{
	}

	Vector(const Vector<T>& v)
		: _vals(NULL), _size(0), _allocated(0)
	{
		copy(v);
	}

	explicit Vector(rhuint initSize, const T& fill = T())
		: _vals(NULL), _size(0), _allocated(0)
	{
		resize(initSize, fill);
	}

	~Vector()
	{
		clear();
		rhdelete(_vals);
	}

	Vector& operator=(const Vector& rhs)
	{
		copy(rhs);
		return *this;
	}

// Operations
	void copy(const Vector<T>& v)
	{
		resize(v._size);
		for(rhuint i = 0; i < v._size; i++) {
			new ((void *)&_vals[i]) T(v._vals[i]);
		}
		_size = v._size;
	}

	void reserve(rhuint newsize)
	{
		if(newsize > size())
			_realloc(newsize);
	}

	// TODO: For performance reason, don't use default fill on POD type
	void resize(rhuint newsize, const T& fill = T())
	{
		if(newsize > _allocated)
			_realloc(newsize);
		if(newsize > _size) {
			while(_size < newsize) {
				new ((void *)&_vals[_size]) T(fill);
				++_size;
			}
		}
		else{
			for(rhuint i = newsize; i < _size; i++) {
				_vals[i].~T();
			}
			_size = newsize;
		}
	}

	void clear()
	{
		// NOTE: There are 2 choices here, to call the destructor in
		// ascending or decending order.
		// decending is more correct, but ascending is faster
		for(rhuint i = 0; i < _size; i++)
			_vals[i].~T();
		_size = 0;
	}

	void shrinktofit() { if(_size > 4) { _realloc(_size); } }

	T& push_back(const T& val = T())
	{
		if(_allocated <= _size) {
			_realloc(_size * 2);
		}
		return *(new ((void *)&_vals[_size++]) T(val));
	}

	void pop_back() { RHASSERT(_size > 0); _size--; _vals[_size].~T(); }

	void insert(rhuint idx, const T& val)
	{
		resize(_size + 1);
		for(rhuint i = _size - 1; i > idx; i--) {
			_vals[i] = _vals[i - 1];
		}
		_vals[idx] = val;
	}

	void insert(iterator position, T* first, T* last)
	{
		RHASSERT(_vals <= position && position <= _vals + _size);
		RHASSERT(first < last);

		const unsigned idx = position - _vals;
		const unsigned amount = last - first;
		const unsigned oldSize = _size;
		resize(_size + amount);

		memmove(_vals + idx + amount, _vals + idx, oldSize - idx);
		memcpy(_vals + idx, first, amount);
	}

	void remove(rhuint idx)
	{
		RHASSERT(idx < _size);
		_vals[idx].~T();
		if(idx < (_size - 1)) {
			memcpy(&_vals[idx], &_vals[idx+1], sizeof(T) * (_size - idx - 1));
		}
		--_size;
	}

	void swap(Vector& rhs)
	{
		T* v = _vals; _vals = rhs._vals; rhs._vals = v;
		rhuint s = _size; _size = rhs._size; rhs._size = s;
		rhuint a = _allocated; _allocated = rhs._allocated; rhs._allocated = a;
	}

// Attributes
	iterator begin() { return _vals; }
	const_iterator begin() const { return _vals; }

	iterator end() { return _vals + _size; }
	const_iterator end() const { return _vals + _size; }

	rhuint size() const { return _size; }

	bool empty() const { return (_size <= 0); }

	rhuint capacity() { return _allocated; }

	T& top() { RHASSERT(_size > 0); return _vals[_size - 1]; }
	const T& top() const { RHASSERT(_size > 0); return _vals[_size - 1]; }

	T& front() { RHASSERT(_size > 0); return _vals[0]; }
	const T& front() const { RHASSERT(_size > 0); return _vals[0]; }

	T& back() { RHASSERT(_size > 0); return _vals[_size - 1]; }
	const T& back() const { RHASSERT(_size > 0); return _vals[_size - 1]; }

	T& at(rhuint pos) { RHASSERT(pos < _size); return _vals[pos]; }
	const T& at(rhuint pos) const { RHASSERT(pos < _size); return _vals[pos]; }

	T& operator[](rhuint pos) { RHASSERT(pos < _size); return _vals[pos]; }
	const T& operator[](rhuint pos) const { RHASSERT(pos < _size); return _vals[pos]; }

	T* _vals;

protected:
	void _realloc(rhuint newsize)
	{
		newsize = (newsize > 0) ? newsize:4;
		_vals = rhrenew(_vals, _allocated, newsize);
		_allocated = newsize;
	}

	rhuint _size;
	rhuint _allocated;
};	// Vector

// Similar to vector class, but with a pre-allocated buffer to 
// minimize memory allocation
template<typename T, unsigned PreAllocSize> class PreAllocVector
{
public:
	typedef T* iterator;
	typedef const T* const_iterator;

	PreAllocVector()
		: _vals((T*)_buffer), _size(0), _allocated(PreAllocSize)
	{
	}

	// This non template copy constructor is needed, for the
	// placement new copy operator to work correctly
	PreAllocVector(const PreAllocVector<T, PreAllocSize>& v)
		: _vals((T*)_buffer), _size(0), _allocated(PreAllocSize)
	{
		copy(v);
	}

	template<unsigned N>
	PreAllocVector(const PreAllocVector<T, N>& v)
		: _vals((T*)_buffer), _size(0), _allocated(PreAllocSize)
	{
		copy(v);
	}

	explicit PreAllocVector(rhuint initSize, const T& fill = T())
		: _vals((T*)_buffer), _size(0), _allocated(PreAllocSize)
	{
		resize(initSize, fill);
	}

	~PreAllocVector()
	{
		clear();
		if(_vals != (T*)_buffer)
			rhdelete(_vals);
	}

	PreAllocVector& operator=(const PreAllocVector& rhs)
	{
		copy(rhs);
		return *this;
	}

// Operations
	template<unsigned N>
	void copy(const PreAllocVector<T, N>& v)
	{
		resize(v._size);
		for(rhuint i = 0; i < v._size; i++) {
			new ((void *)&_vals[i]) T(v._vals[i]);
		}
		_size = v._size;
	}

	void reserve(rhuint newsize)
	{
		if(newsize > size())
			_realloc(newsize);
	}

	// TODO: For performance reason, don't use default fill on POD type
	void resize(rhuint newsize, const T& fill = T())
	{
		if(newsize > _allocated)
			_realloc(newsize);
		if(newsize > _size) {
			while(_size < newsize) {
				new ((void *)&_vals[_size]) T(fill);
				++_size;
			}
		}
		else{
			for(rhuint i = newsize; i < _size; i++) {
				_vals[i].~T();
			}
			_size = newsize;
		}
	}

	void clear()
	{
		// NOTE: There are 2 choices here, to call the destructor in
		// ascending or decending order.
		// decending is more correct, but ascending is faster
		for(rhuint i = 0; i < _size; i++)
			_vals[i].~T();
		_size = 0;
	}

	void shrinktofit() { _realloc(_size); }

	T& push_back(const T& val = T())
	{
		if(_allocated <= _size) {
			_realloc(_size * 2);
		}
		return *(new ((void *)&_vals[_size++]) T(val));
	}

	void pop_back() { RHASSERT(_size > 0); _size--; _vals[_size].~T(); }

	void insert(rhuint idx, const T& val)
	{
		resize(_size + 1);
		for(rhuint i = _size - 1; i > idx; i--) {
			_vals[i] = _vals[i - 1];
		}
		_vals[idx] = val;
	}

	void insert(iterator position, T* first, T* last)
	{
		RHASSERT(_vals <= position && position <= _vals + _size);
		RHASSERT(first < last);

		const unsigned idx = position - _vals;
		const unsigned amount = last - first;
		const unsigned oldSize = _size;
		resize(_size + amount);

		memmove(_vals + idx + amount, _vals + idx, oldSize - idx);
		memcpy(_vals + idx, first, amount);
	}

	void remove(rhuint idx)
	{
		RHASSERT(idx < _size);
		_vals[idx].~T();
		if(idx < (_size - 1)) {
			memcpy(&_vals[idx], &_vals[idx+1], sizeof(T) * (_size - idx - 1));
		}
		--_size;
	}

// Attributes
	iterator begin() { return _vals; }
	const_iterator begin() const { return _vals; }

	iterator end() { return _vals + _size; }
	const_iterator end() const { return _vals + _size; }

	rhuint size() const { return _size; }

	bool empty() const { return (_size <= 0); }

	rhuint capacity() { return _allocated; }

	T& top() { RHASSERT(_size > 0); return _vals[_size - 1]; }
	const T& top() const { RHASSERT(_size > 0); return _vals[_size - 1]; }

	T& front() { RHASSERT(_size > 0); return _vals[0]; }
	const T& front() const { RHASSERT(_size > 0); return _vals[0]; }

	T& back() { RHASSERT(_size > 0); return _vals[_size - 1]; }
	const T& back() const { RHASSERT(_size > 0); return _vals[_size - 1]; }

	T& at(rhuint pos) { RHASSERT(pos < _size); return _vals[pos]; }
	const T& at(rhuint pos) const { RHASSERT(pos < _size); return _vals[pos]; }

	T& operator[](rhuint pos) { RHASSERT(pos < _size); return _vals[pos]; }
	const T& operator[](rhuint pos) const { RHASSERT(pos < _size); return _vals[pos]; }

	T* _vals;

protected:
	void _realloc(rhuint newsize)
	{
		// Transit from dynamic to static
		if(newsize <= PreAllocSize && PreAllocSize < _allocated) {
			memcpy(_buffer, _vals, sizeof(T) * _size);
			rhdelete(_vals);
			_vals = (T*)_buffer;
			_allocated = PreAllocSize;
		}
		// Transit from static to dynamic
		else {
			_vals = (_vals == (T*)_buffer) ? NULL : _vals;
			_vals = rhrenew(_vals, _allocated, newsize);

			if(_allocated == PreAllocSize)
				memcpy(_vals, _buffer, sizeof(T) * _size);

			_allocated = newsize;
		}
	}

	rhuint _size;
	rhuint _allocated;
	char* _buffer[PreAllocSize * sizeof(T)];
};	// PreAllocVector

template<typename T, typename V>
T* rhFind(T* begin, T* end, const V& v)
{
	for(T* i=begin; i!=end; ++i)
		if(*i == v) return i;
	return NULL;
}

#endif	// __VECTOR_H__
