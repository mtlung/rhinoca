template<class T> inline
void roSwap(ro::IArray<T>& lhs, ro::IArray<T>& rhs)
{
	roSwap(lhs._size, rhs._size);
	roSwap(lhs._capacity, rhs._capacity);
	roSwap(lhs._data, rhs._data);
}

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
	// Simply swap pointer and sizes if both are dynamic
	if(lhs._isUsingDynamic() && rhs._isUsingDynamic()) {
		roSwap(lhs._size, rhs._size);
		roSwap(lhs._capacity, rhs._capacity);
		roSwap(lhs._data, rhs._data);
	}
	// Otherwise swap element by element
	else {
		roSize lSize = lhs._size;
		roSize rSize = rhs._size;
		roSize size2 = roMaxOf2(lSize, rSize);
		lhs.resize(size2);
		rhs.resize(size2);

		for(roSize i=0; i<size2; ++i)
			roSwap(lhs[i], rhs[i]);

		lhs.resize(rSize);
		rhs.resize(lSize);
	}
}

namespace ro {

template<class T, roSize N>
void StaticArray<T,N>::assign(const T& fill)
{
	for(roSize i=0; i<size(); ++i)
		data[i] = fill;
}


// ----------------------------------------------------------------------

template<class T>
Status IArray<T>::copy(const IArray<T>& src)
{
	Status st = this->resize(src._size);
	if(!st) return st;
	for(roSize i = 0; i < src._size; ++i) {
		_data[i] = src._data[i];
	}
	return Status::ok;
}

template<class T>
Status IArray<T>::assign(const T* srcBegin, roSize count)
{
	clear();
	insert(0, srcBegin, count);
	return Status::ok;
}

template<class T>
Status IArray<T>::assign(const T* srcBegin, const T* srcEnd)
{
	clear();
	insert(0, srcBegin, srcEnd);
	return Status::ok;
}

template<class T> roFORCEINLINE
Status IArray<T>::resize(roSize newSize, const T& fill)
{
	if(newSize > _capacity) {
		Status st = reserve(roMaxOf2(newSize, _capacity + _capacity/2), false);	// Extend the capacity by 1.5x
		if(!st) return st;
	}

	roSize sz = _size;
	if(newSize > sz) {
		while(sz < newSize) {
			new ((void *)&_data[sz]) T(fill);
			++sz;
		}
	}
	else {
		if(!TypeOf<T>::isPOD())
		for(roSize i = newSize; i < sz; ++i) {
			_data[i].~T();
		}
	}

	_size = newSize;
	return Status::ok;
}

template<class T>
Status IArray<T>::incSize(roSize newSize, const T& fill)
{
	return this->resize(_size + newSize, fill);
}

template<class T>
Status IArray<T>::resizeNoInit(roSize newSize)
{
	if(newSize > _capacity) {
		Status st = reserve(roMaxOf2(newSize, _capacity + _capacity/2), false);	// Extend the size by 1.5x
		if(!st) return st;
	}

	_size = newSize;
	return Status::ok;
}

template<class T>
Status IArray<T>::incSizeNoInit(roSize newSize)
{
	return this->resizeNoInit(_size + newSize);
}

template<class T>
void IArray<T>::clear()
{
	this->resize(0);
}

template<class T>
void IArray<T>::condense()
{
	reserve(_size, true);
}

template<class T> roFORCEINLINE
Status IArray<T>::pushBack(const T& fill)
{
	return this->resize(_size + 1, fill);
}

template<class T> roFORCEINLINE
Status IArray<T>::pushBack(const T* srcBegin, roSize count)
{
	return insert(this->_size, srcBegin, count);
}

template<class T>
Status IArray<T>::pushBackBySwap(const T& val)
{
	Status st = this->resize(_size + 1);
	if(!st) return st;
	roSwap(back(), const_cast<T&>(val));
	return st;
}

template<class T>
Status IArray<T>::insert(roSize idx, const T& val)
{
	roAssert(idx <= _size);
	Status st = this->resize(_size + 1);
	if(!st) return st;
	for(roSize i = _size - 1; i > idx; --i)
		_data[i] = _data[i - 1];

	_data[idx] = val;
	return st;
}

template<class T>
Status IArray<T>::insert(roSize idx, const T* srcBegin, roSize count)
{
	return insert(idx, srcBegin, srcBegin + count);
}

template<class T>
Status IArray<T>::insert(roSize idx, const T* srcBegin, const T* srcEnd)
{
	roAssert(idx <= _size);
	roAssert(srcBegin <= srcEnd);
	roSize inc = srcEnd - srcBegin;

	if(inc == 0) return Status::ok;

	Status st = this->resize(_size + inc);
	if(!st) return st;

	for(roSize i = _size - 1; i >= idx + inc; --i)
		_data[i] = _data[i - inc];

	for(T* src = (T*)srcBegin, *dst = _data + idx; src != srcEnd; ++src, ++dst)
		*dst = *src;

	return st;
}

template<class T>
Status IArray<T>::insertUnique(roSize idx, const T& val)
{
	Status st = Status::ok;
	if(find(val) != NULL)
		return st;
	return insert(idx, val);
}

template<class T>
Status IArray<T>::insertSorted(const T&val)
{
	if(T* p = roLowerBound(_data, _size, val)) {
		roAssert(!(*p < val));
		return this->insert(p - _data, val);
	} else
		return this->pushBack(val);
}

template<class T>
Status IArray<T>::insertSorted(const T& val, bool(*less)(const T&, const T&))
{
	if(T* p = roLowerBound(_data, _size, val, less)) {
		roAssert(!less(*p, val));
		return this->insert(p - _data, val);
	} else
		return this->pushBack(val);
}

template<class T>
void IArray<T>::popBack()
{
	if(_size > 0)
		this->resize(_size - 1);
}

template<class T>
void IArray<T>::remove(roSize idx)
{
	roAssert(idx < _size);
	if(idx >= _size) return;

	_data[idx].~T();
	if(idx < _size - 1) {
		if(TypeOf<T>::isPOD())
			roMemmov(&_data[idx], &_data[idx+1], sizeof(T) * (_size - idx - 1));
		else {
			new ((void *)&_data[idx]) T;
			for(roSize i=idx; i<_size-1; ++i)	// Move object forward
				roSwap(_data[i], _data[i+1]);
			_data[_size-1].~T();
		}
	}
	--_size;
}

template<class T>
void IArray<T>::removeBySwap(roSize idx)
{
	roAssert(idx < _size);
	if(idx >= _size) return;

	if(_size > 1)
		roSwap(_data[idx], back());

	this->popBack();
}

template<class T>
bool IArray<T>::removeByKey(const T& key)
{
	T* v = this->find(key);
	if(!v) return false;

	roAssert(v >= begin() && v < end());
	this->remove(v - begin());
	return true;
}

template<class T>
bool IArray<T>::removeAllByKey(const T& key)
{
	bool ret = false;
	for(roSize i=0; i<this->_size;) {
		if(_data[i] == key) {
			this->remove(i);
			ret = true;
		}
		else
			++i;
	}

	return ret;
}

template<class T>
T* IArray<T>::find(const T& key)
{
	return roArrayFind(this->_data, this->_size, key);
}

template<class T>
const T* IArray<T>::find(const T& key) const
{
	return roArrayFind(this->_data, this->_size, key);
}

template<class T> template<class K>
T* IArray<T>::find(const K& key, bool(*equal)(const T&, const K&))
{
	return roArrayFind(this->_data, this->_size, key, equal);
}

template<class T>  template<class K>
const T* IArray<T>::find(const K& key, bool(*equal)(const T&, const K&)) const
{
	return roArrayFind(this->_data, this->_size, key, equal);
}


// ----------------------------------------------------------------------

template<class T> roFORCEINLINE
Status Array<T>::reserve(roSize newCapacity, bool force)
{
	newCapacity = roMaxOf2(newCapacity, this->size());
	if(newCapacity == 0) return Status::ok;

	if(!force && this->_capacity >= newCapacity)
		return Status::ok;

	T* newPtr = NULL;
	if(TypeOf<T>::isPOD()) {
		newPtr = roRealloc(this->_data, this->_capacity * sizeof(T), newCapacity * sizeof(T)).template cast<T>();
		if(!newPtr) return Status::not_enough_memory;
	}
	else {
		newPtr = roMalloc(newCapacity * sizeof(T)).template cast<T>();
		// Move the objects from old memory to new memory
		for(roSize i=0; i<this->_size; ++i) {
			new ((void *)&newPtr[i]) T;
			roSwap(this->_data[i], newPtr[i]);
			_data[i].~T();
		}
		roFree(this->_data);
	}

	this->_data = newPtr;
	this->_capacity = newCapacity;
	return Status::ok;
}


// ----------------------------------------------------------------------

template<class T>
inline void _tinyArrayMoveObjects(T* oldPtr, T* newPtr, roSize size)
{
	if(TypeOf<T>::isPOD()) {
		if(oldPtr && newPtr)
			roMemcpy(newPtr, oldPtr, sizeof(T) * size);
	}
	else for(roSize i=0; i<size; ++i) {
		new (newPtr + i) T;
		roSwap(oldPtr[i], newPtr[i]);
		(oldPtr[i]).~T();
	}
}

template<class T, roSize PreAllocCount>
Status TinyArray<T,PreAllocCount>::reserve(roSize newCapacity, bool force)
{
	roAssert(this->_capacity >= PreAllocCount || this->_capacity == 0);

	newCapacity = roMaxOf3(newCapacity, PreAllocCount, this->size());

	if(!force && this->_capacity >= newCapacity)
		return Status::ok;

	// Transit from dynamic to static
	// NOTE: The check for this->_capacity == 0 make this class "zero memory initialization" friendly
	if(newCapacity <= PreAllocCount && (_isUsingDynamic() || this->_capacity == 0)) {
		T* oldPtr = this->_data;
		T* newPtr = (T*)this->_buffer;

		_tinyArrayMoveObjects(oldPtr, newPtr, this->_size);

		roFree(oldPtr);
		this->_data = newPtr;
		this->_capacity = PreAllocCount;
	}
	// Transit from static to dynamic
	else if(newCapacity > PreAllocCount && !_isUsingDynamic()) {
		T* oldPtr = (T*)this->_buffer;
		T* newPtr = roMalloc(newCapacity * sizeof(T)).template cast<T>();
		if(!newPtr) return Status::not_enough_memory;

		_tinyArrayMoveObjects(oldPtr, newPtr, this->_size);

		this->_data = newPtr;
		this->_capacity = newCapacity;
	}
	// Enlarge/shrink dynamic memory
	else if(newCapacity != this->_capacity) {
		T* oldPtr = this->_data;
		T* newPtr = NULL;

		if(TypeOf<T>::isPOD()) {
			newPtr = roRealloc(oldPtr, this->_capacity * sizeof(T), newCapacity * sizeof(T)).template cast<T>();
			if(!newPtr) return Status::not_enough_memory;
		}
		else {
			newPtr = roMalloc(newCapacity * sizeof(T)).template cast<T>();

			roAssert(newCapacity >= this->_size);
			_tinyArrayMoveObjects(oldPtr, newPtr, this->_size);

			roFree(oldPtr);
		}

		this->_data = newPtr;
		this->_capacity = newCapacity;
	}

	roAssert(this->_capacity >= PreAllocCount);
	return Status::ok;
}

template<class T>
Status ExtArray<T>::reserve(roSize newCapacity, bool force)
{
	newCapacity = roMaxOf2(newCapacity, this->size());

	if(!force && this->_capacity >= newCapacity)
		return Status::ok;

	if(newCapacity > this->_capacity)
		return Status::not_enough_memory;
}

}	// namespace ro
