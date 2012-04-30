template<class T> inline
void roSwap(ro::Array<T>& lhs, ro::Array<T>& rhs)
{
	roSwap(lhs._size, rhs._size);
	roSwap(lhs._capacity, rhs._capacity);
	roSwap(lhs._data, rhs._data);
}

template<class T, roSize N> inline
void roSwap(ro::TinyArray<T,N>& lhs, ro::TinyArray<T,N>& rhs)
{
	roSwap(lhs._size, rhs._size);
	roSwap(lhs._capacity, rhs._capacity);
	roSwap(lhs._data, rhs._data);
}

template<class T, roSize N> inline
void roOnMemMove(ro::TinyArray<T,N>& ary, void* newMemLocation)
{
	if(ary._capacity <= N) {	// Array is using it's static buffer
		ary._data = (T*)ary._buffer;
		for(roSize i=0; i<ary._size; ++i)
			roOnMemMove(ary._data[i], &ary._data[i]);
	}
}

namespace ro {

template<class T>
T* arrayFind(T* begin, T* end, const T& val)
{
	for(T* i=begin; i!=end; ++i)
		if(*i == val)
			return i;
	return NULL;
}

template<class T>
T* arrayFind(T* begin, roSize count, const T& val)
{
	return arrayFind(begin, begin + count, val);
}

template<class T, roSize N>
void StaticArray<T,N>::assign(const T& fill)
{
	for(roSize i=0; i<size(); ++i)
		data[i] = fill;
}


// ----------------------------------------------------------------------

template<class T, class S>
Status IArray<T,S>::copy(const S& src)
{
	Status st = resize(src._size);
	if(!st) return st;
	for(roSize i = 0; i < src._size; ++i) {
		_data[i] = src._data[i];
	}
	return Status::ok;
}

template<class T, class S>
Status IArray<T,S>::resize(roSize newSize, const T& fill)
{
	if(newSize > _capacity) {
		Status st = static_cast<S&>(*this).reserve(roMaxOf2(newSize, _size*3/2));	// Extend the size by 1.5x
		if(!st) return st;
	}

	if(newSize > _size) {
		while(_size < newSize) {
			new ((void *)&_data[_size]) T(fill);
			++_size;
		}
	}
	else{
		if(!TypeOf<T>::isPOD())
		for(roSize i = newSize; i < _size; ++i) {
			_data[i].~T();
		}
		_size = newSize;
	}

	return Status::ok;
}

template<class T, class S>
Status IArray<T,S>::incSize(roSize newSize, const T& fill)
{
	return resize(_size + newSize, fill);
}

template<class T, class S>
void IArray<T,S>::clear()
{
	resize(0);
}

template<class T, class S>
void IArray<T,S>::condense()
{
	static_cast<S&>(*this).reserve(_size);
}

template<class T, class S>
T& IArray<T,S>::pushBack(const T& fill)
{
	resize(_size + 1, fill);
	return back();
}

template<class T, class S>
T& IArray<T,S>::pushBackBySwap(const T& val)
{
	resize(_size + 1);
	roSwap(back(), const_cast<T&>(val));
	return back();
}

template<class T, class S>
T& IArray<T,S>::insert(roSize idx, const T& val)
{
	resize(_size + 1);
	for(roSize i = _size - 1; i > idx; --i) {
		_data[i] = _data[i - 1];
	}
	_data[idx] = val;
	return _data[idx];
}

template<class T, class S>
T& IArray<T,S>::insert(roSize idx, const T* srcBegin, const T* srcEnd)
{
	roAssert(srcBegin <= srcEnd);
	resize(_size + (srcEnd - srcBegin));
	for(roSize i = _size - 1; i > idx; --i) {
		_data[i] = _data[i - 1];
	}

	for(const T* src = srcBegin, *dst = _data + idx; src != srcEnd; ++src, ++dst)
		*dst = *src;

	return _data[idx];
}

template<class T, class S>
void IArray<T,S>::popBack()
{
	if(_size > 0)
		resize(_size - 1);
}

template<class T, class S>
void IArray<T,S>::remove(roSize idx)
{
	roAssert(idx < _size);
	if(idx >= _size) return;

	_data[idx].~T();
	if(idx < _size - 1) {
		memmove(&_data[idx], &_data[idx+1], sizeof(T) * (_size - idx - 1));
		if(!TypeOf<T>::isPOD()) for(roSize i=idx; i<_size-1; ++i)	// Notify the object that it's memory is moved
			roOnMemMove(_data[i], &_data[i]);
	}
	--_size;
}

template<class T, class S>
void IArray<T,S>::removeBySwap(roSize idx)
{
	roAssert(idx < _size);
	if(idx >= _size) return;

	if(_size > 1)
		roSwap(_data[idx], back());

	popBack();
}

template<class T, class S>
T* IArray<T,S>::find(const T& val) const
{
	return arrayFind(begin, end, val);
}


// ----------------------------------------------------------------------

template<class T>
Status Array<T>::reserve(roSize newCapacity)
{
	newCapacity = roMaxOf2(newCapacity, size());
	if(newCapacity == 0) return Status::ok;

	T* newPtr = roRealloc(_data, newCapacity * sizeof(T), newCapacity * sizeof(T)).cast<T>();
	if(!newPtr) return Status::not_enough_memory;

	if(!TypeOf<T>::isPOD() && newPtr != _data) for(roSize i=0; i<_size; ++i)	// Notify the object that it's memory is moved
		roOnMemMove(_data[i], newPtr + i);
	_data = newPtr;
	_capacity = newCapacity;
	return Status::ok;
}


// ----------------------------------------------------------------------

template<class T, roSize PreAllocCount>
Status TinyArray<T,PreAllocCount>::reserve(roSize newSize)
{
	newSize = roMaxOf2(newSize, size());
	bool moved = false;

	// Transit from dynamic to static
	if(newSize <= PreAllocCount && PreAllocCount < _capacity) {
		roMemcpy(_buffer, _data, sizeof(T) * _size);
		roFree(_data);
		_data = (T*)_buffer;
		_capacity = PreAllocCount;
		moved = true;
	}
	// Transit from static to dynamic
	else {
		T* oldPtr = (_data == (T*)_buffer) ? NULL : _data;
		T* newPtr = roRealloc(oldPtr, _capacity, newSize * sizeof(T)).cast<T>();
		if(!newPtr) return Status::not_enough_memory;

		moved = _data != oldPtr;

		if(_capacity == PreAllocCount) {
			roMemcpy(newPtr, _buffer, sizeof(T) * _size);
			moved = true;
		}

		_data = newPtr;
		_capacity = newSize;
	}

	if(!TypeOf<T>::isPOD() && moved) for(roSize i=0; i<_size; ++i)	// Notify the object that it's memory is moved
		roOnMemMove(_data[i], &_data[i]);

	return Status::ok;
}

}	// namespace ro
