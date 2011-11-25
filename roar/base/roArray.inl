#ifndef _NEW_
// Define our own placement new and delete operator such that we need not to include <new>
//#include <new>
inline void* operator new(size_t, void* where) { return where; }
inline void operator delete(void*, void*) {}
#endif	// _NEW_

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
void roOnMemMove(ro::TinyArray<T,N>& obj, void* newMemLocation)
{
}

namespace ro {

template<class A>
void arrayResize(A& ary, roSize newSize)
{
	typedef typename A::T T;
	arrayResize(ary, newSize, T());
}

template<class A>
void arrayResize(A& ary, roSize newSize, const typename A::T& fill)
{
	typedef typename A::T T;

	if(newSize > ary._capacity)
		ary.setCapacity(roMaxOf2(newSize, ary._size*3/2));	// Extend the size by 1.5x
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
typename A::T& arrayIncSize(A& ary)
{
	arrayResize(ary, ary.size() + 1);
	return ary.back();
}

template<class A>
typename A::T& arrayIncSize(A& ary, const typename A::T& fill)
{
	arrayResize(ary, ary.size() + 1, fill);
	return ary.back();
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
void arrayAppendBySwap(A& ary, typename A::T& val)
{
	arrayResize(ary, ary._size + 1);
	roSwap(ary.back(), val);
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
	roAssert(idx < ary._size);
	if(idx >= ary._size) return;

	typedef typename A::T T;
	ary[idx].~T();
	if(idx < ary._size - 1) {
		memmove(&ary[idx], &ary[idx+1], sizeof(A::T) * (ary._size - idx - 1));
		if(!TypeOf<T>::isPOD()) for(roSize i=idx; i<ary._size-1; ++i)	// Notify the object that it's memory is moved
			roOnMemMove(ary[i], &ary[i]);
	}
	--ary._size;
}

template<class A>
void arrayRemoveBySwap(A& ary, roSize idx)
{
	roAssert(idx < ary._size);
	if(idx >= ary._size) return;

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

template<class T>
T* arrayFind(T* begin, T* end, const T& val)
{
	for(T* i=begin; i!=end; ++i)
		if(*i == val)
			return i;
	return NULL;
}

}	// namespace ro
