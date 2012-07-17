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
	if(ary._isUsingDynamic())
		return;

	ary._data = (T*)ary._buffer;
	for(roSize i=0; i<ary._size; ++i)
		roOnMemMove(ary._data[i], &ary._data[i]);
}

namespace ro {

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
	Status st = _typedThis().resize(src._size);
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
		Status st = _typedThis().reserve(roMaxOf2(newSize, _size*3/2));	// Extend the size by 1.5x
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
	return _typedThis().resize(_size + newSize, fill);
}

template<class T, class S>
Status IArray<T,S>::resizeNoInit(roSize newSize)
{
	if(newSize > _capacity) {
		Status st = _typedThis().reserve(roMaxOf2(newSize, _size*3/2));	// Extend the size by 1.5x
		if(!st) return st;
	}

	_size = newSize;
	return Status::ok;
}

template<class T, class S>
Status IArray<T,S>::incSizeNoInit(roSize newSize)
{
	return _typedThis().resizeNoInit(_size + newSize);
}

template<class T, class S>
void IArray<T,S>::clear()
{
	_typedThis().resize(0);
}

template<class T, class S>
void IArray<T,S>::condense()
{
	_typedThis().reserve(_size);
}

template<class T, class S>
T& IArray<T,S>::pushBack(const T& fill)
{
	_typedThis().resize(_size + 1, fill);
	return back();
}

template<class T, class S>
T& IArray<T,S>::pushBackBySwap(const T& val)
{
	_typedThis().resize(_size + 1);
	roSwap(back(), const_cast<T&>(val));
	return back();
}

template<class T, class S>
T& IArray<T,S>::insert(roSize idx, const T& val)
{
	roAssert(idx <= _size);
	_typedThis().resize(_size + 1);
	for(roSize i = _size - 1; i > idx; --i)
		_data[i] = _data[i - 1];

	_data[idx] = val;
	return _data[idx];
}

template<class T, class S>
T& IArray<T,S>::insert(roSize idx, const T* srcBegin, const T* srcEnd)
{
	roAssert(idx <= _size);
	roAssert(srcBegin <= srcEnd);
	roSize inc = srcEnd - srcBegin;
	if(inc == 0)
		return _data[idx];

	_typedThis().resize(_size + inc);
	for(roSize i = _size - 1; i >= idx + inc; --i)
		_data[i] = _data[i - inc];

	for(T* src = (T*)srcBegin, *dst = _data + idx; src != srcEnd; ++src, ++dst)
		*dst = *src;

	return _data[idx];
}

template<class T, class S>
T& IArray<T,S>::insertSorted(const T&val)
{
	if(T* p = roLowerBound(_data, _size, val)) {
		roAssert(!(*p < val));
		return _typedThis().insert(p - _data, val);
	} else
		return _typedThis().pushBack(val);
}

template<class T, class S>
T& IArray<T,S>::insertSorted(const T& val, bool(*less)(const T&, const T&))
{
	if(T* p = roLowerBound(_data, _size, val, less)) {
		roAssert(!less(*p, val));
		return _typedThis().insert(p - _data, val);
	} else
		return _typedThis().pushBack(val);
}

template<class T, class S>
void IArray<T,S>::popBack()
{
	if(_size > 0)
		_typedThis().resize(_size - 1);
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

	_typedThis().popBack();
}

template<class T, class S>
void IArray<T,S>::removeByKey(const T& key)
{
	T* v = _typedThis().find(key);
	if(!v) return;

	roAssert(v >= begin() && v < end());
	_typedThis().remove(v - begin());
}

template<class T, class S>
T* IArray<T,S>::find(const T& key)
{
	return roArrayFind(this->_data, this->_size, key);
}

template<class T, class S>
const T* IArray<T,S>::find(const T& key) const
{
	return roArrayFind(this->_data, this->_size, key);
}

template<class T, class S> template<class K>
T* IArray<T,S>::find(const K& key, bool(*equal)(const T&, const K&))
{
	return roArrayFind(this->_data, this->_size, key, equal);
}

template<class T, class S>  template<class K>
const T* IArray<T,S>::find(const K& key, bool(*equal)(const T&, const K&)) const
{
	return roArrayFind(this->_data, this->_size, key, equal);
}


// ----------------------------------------------------------------------

template<class T>
Status Array<T>::reserve(roSize newCapacity)
{
	newCapacity = roMaxOf2(newCapacity, this->size());
	if(newCapacity == 0) return Status::ok;

	T* newPtr = roRealloc(this->_data, newCapacity * sizeof(T), newCapacity * sizeof(T)).template cast<T>();
	if(!newPtr) return Status::not_enough_memory;

	if(!TypeOf<T>::isPOD() && newPtr != this->_data) for(roSize i=0; i<this->_size; ++i)	// Notify the object that it's memory is moved
		roOnMemMove(this->_data[i], newPtr + i);
	this->_data = newPtr;
	this->_capacity = newCapacity;
	return Status::ok;
}


// ----------------------------------------------------------------------

template<class T, roSize PreAllocCount>
Status TinyArray<T,PreAllocCount>::reserve(roSize newSize)
{
	roAssert(this->_capacity >= PreAllocCount || this->_capacity == 0);

	newSize = roMaxOf2(newSize, this->size());
	bool moved = false;

	// Transit from dynamic to static
	// NOTE: The check for this->_capacity == 0 make this class "zero memory initialization" friendly
	if(newSize <= PreAllocCount && (_isUsingDynamic() || this->_capacity == 0)) {
		roMemcpy(this->_buffer, this->_data, sizeof(T) * this->_size);
		roFree(this->_data);
		this->_data = (T*)this->_buffer;
		this->_capacity = PreAllocCount;
		moved = true;
	}
	// Transit from static to dynamic
	else {
		T* oldPtr = (this->_data == (T*)this->_buffer) ? NULL : this->_data;
		T* newPtr = roRealloc(oldPtr, this->_capacity, newSize * sizeof(T)).template cast<T>();
		if(!newPtr) return Status::not_enough_memory;

		if(oldPtr != newPtr) {
			roMemcpy(newPtr, this->_buffer, sizeof(T) * this->_size);
			moved = true;
		}

		this->_data = newPtr;
		this->_capacity = newSize;
	}

	roAssert(this->_capacity >= PreAllocCount);

	if(!TypeOf<T>::isPOD() && moved) for(roSize i=0; i<this->_size; ++i)	// Notify the object that it's memory is moved
		roOnMemMove(this->_data[i], &this->_data[i]);

	return Status::ok;
}

}	// namespace ro
