#ifndef __VECTOR_H__
#define __VECTOR_H__

#include "common.h"
#include <new>	// Visual studio need this header for placement new!

// Mini vector class, supports objects by value
template<typename T> class Vector
{
public:
	Vector()
	{
		_vals = NULL;
		_size = 0;
		_allocated = 0;
	}

	Vector(const Vector<T>& v) { copy(v); }

	explicit Vector(rhuint initSize, const T& fill = T()) {
		resize(initSize, fill);
	}

	~Vector()
	{
		for(rhuint i = 0; i < _size; i++) {
			_vals[i].~T();
		}
		rhdelete(_vals);
	}

	void copy(const Vector<T>& v)
	{
		resize(v._size);
		for(rhuint i = 0; i < v._size; i++) {
			new ((void *)&_vals[i]) T(v._vals[i]);
		}
		_size = v._size;
	}

	void reserve(rhuint newsize) { _realloc(newsize); }

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

	void shrinktofit() { if(_size > 4) { _realloc(_size); } }

	T& top() const { ASSERT(_size > 0); return _vals[_size - 1]; }

	inline rhuint size() const { return _size; }

	bool empty() const { return (_size <= 0); }

	inline T& push_back(const T& val = T())
	{
		if(_allocated <= _size) {
			_realloc(_size * 2);
		}
		return *(new ((void *)&_vals[_size++]) T(val));
	}

	inline void pop_back() { ASSERT(_size > 0); _size--; _vals[_size].~T(); }

	void insert(rhuint idx, const T& val)
	{
		resize(_size + 1);
		for(rhuint i = _size - 1; i > idx; i--) {
			_vals[i] = _vals[i - 1];
		}
    	_vals[idx] = val;
	}

	void remove(rhuint idx)
	{
		ASSERT(idx < _size);
		_vals[idx].~T();
		if(idx < (_size - 1)) {
			memcpy(&_vals[idx], &_vals[idx+1], sizeof(T) * (_size - idx - 1));
		}
		--_size;
	}

	rhuint capacity() { return _allocated; }

	inline T& back() const { ASSERT(_size > 0); return _vals[_size - 1]; }

	inline T& operator[](rhuint pos) const{ ASSERT(pos < _size); return _vals[pos]; }

	void swap(Vector& rhs)
	{
		T* v = _vals; _vals = rhs._vals; rhs._vals = v;
		rhuint s = _size; _size = rhs._size; rhs._size = s;
		rhuint a = _allocated; _allocated = rhs._allocated; rhs._allocated = a;
	}

	T* _vals;

private:
	void _realloc(rhuint newsize)
	{
		newsize = (newsize > 0)?newsize:4;
		_vals = rhrenew(_vals, _allocated, newsize);
		_allocated = newsize;
	}

	rhuint _size;
	rhuint _allocated;
};	// Vector

#endif
