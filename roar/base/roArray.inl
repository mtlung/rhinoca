template<class A>
void arrayResize(A& ary, roSize newSize, const typename A::T& fill)
{
	typedef typename A::T T;

	if(newSize > ary._capacity)
		ary.setCapacity(newSize);
	if(newSize > ary._size) {
		while(ary._size < newSize) {
			new ((void *)&ary._data[ary._size]) T(fill);
			++ary._size;
		}
	}
	else{
		if(!TypeOf<A::T>::isPOD())
		for(roSize i = newSize; i < ary._size; ++i) {
			ary._data[i].~T();
		}
		ary._size = newSize;
	}
}

template<class A>
void arrayIncSize(A& ary, const typename A::T& fill)
{
	arrayResize(ary, ary.size() + 1, fill);
}

template<class A>
void arrayDecSize(A& ary)
{
	roSize s = ary.size();
	if(s > 0) arrayResize(s - 1);
}

template<class A>
void arrayAppend(A& ary, const typename A::T& val)
{
	arrayResize(ary, ary._size + 1, val);
}

template<class A>
void arrayInsert(A& ary, roSize idx, const typename A::T& val)
{
	arrayResize(ary, ary._size + 1);
	for(roSize i = ary._size - 1; i > idx; --i) {
		ary._vals[i] = ary._vals[i - 1];
	}
	ary._vals[idx] = val;
}

template<class A>
void arrayRemove(A& ary, roSize idx)
{
	roAssert(idx < _size);
	if(idx >= ary._size) return;

	typedef typename A::T T;
	ary[idx].~T();
	if(idx < ary._size - 1)
		memcpy(&ary[idx], &ary[idx+1], sizeof(A::T) * (ary._size - idx - 1));

	ary.popBack();
}

template<class A>
void arrayRemoveBySwap(A& ary, roSize idx)
{
	roAssert(idx < ary._size);
	if(idx >= ary._size) return;

	typedef typename A::T T;
	ary[idx].~T();
	if(ary._size > 1)
		roSwap(ary[idx], ary.back());

	ary.popBack();
}

template<class T>
T* arrayFind(T* begin, roSize count, const T& val)
{
	for(roSize i=0; i<count; ++i)
		if(begin[i] == val)
			return begin + i;
	return NULL;
}
