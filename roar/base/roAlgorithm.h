#ifndef __roAlgorithm_h__
#define __roAlgorithm_h__

#include "../platform/roCompiler.h"

// Searching
template<class T>			T*	roArrayFind	(T* begin, T* end, const T& key);
template<class T>			T*	roArrayFind	(T* begin, roSize count, const T& key);
template<class T, class K>	T*	roArrayFind	(T* begin, roSize count, const K& key, bool(*equal)(const T&, const K&));

template<class T>			T*	roLowerBound(T* ary, roSize count, const T& key);
template<class T, class K>	T*	roLowerBound(T* ary, roSize count, const K& key, bool(*less)(const T&, const K&));
template<class T>			T*	roUpperBound(T* ary, roSize count, const T& key);
template<class T, class K>	T*	roUpperBound(T* ary, roSize count, const K& key, bool(*less)(const K&, const T&));

// Sorting
template<class T>	void roInsertionSort(T* begin, T* end);	/// Stable, O(1) space, O(n2) compare and swaps, adaptive
template<class T>	void roSelectionSort(T* begin, T* end);	/// Not stable, O(1) space, O(n2) compare, O(n) swaps, non-adaptive


template<class T>
T* roArrayFind(T* begin, T* end, const T& key)
{
	for(T* i=begin; i!=end; ++i)
		if(*i == key)
			return i;
	return NULL;
}

template<class T>
const T* roArrayFind(const T* begin, const T* end, const T& key)
{
	return arrayFind((T*)begin, end, key);
}

template<class T, class K>
T* roArrayFind(T* begin, roSize count, const K& key, bool(*equal)(const T&, const K&))
{
	for(T* i=begin; i!=begin + count; ++i)
		if(equal(*i, key))
			return i;
	return NULL;
}

template<class T>
T* roArrayFind(T* begin, roSize count, const T& key)
{
	return roArrayFind(begin, begin + count, key);
}

template<class T>
const T* roArrayFind(const T* begin, roSize count, const T& key)
{
	return roArrayFind(begin, begin + count, key);
}

/// Find first element not before key, using operator<
/// Specifically, it returns the first position where value could be inserted without violating the ordering.
template<class T>
T* roLowerBound(T* ary, roSize count, const T& key)
{
	T* end = ary + count;
	for (; 0 < count; )
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(*mid < key)
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary == end ? NULL : ary;
}

/// Find first element not before key, using predicate function
template<class T, class K>
T* roLowerBound(T* ary, roSize count, const K& key, bool(*less)(const T&, const K&))
{
	T* end = ary + count;
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(less(*mid, key))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary == end ? NULL : ary;
}

/// Find first element that key is before, using operator<
/// Specifically, it returns the last position where value could be inserted without violating the ordering.
template<class T>
T* roUpperBound(T* ary, roSize count, const T& key)
{
	T* end = ary + count;
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(!(key < *mid))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary == end ? NULL : ary;
}

/// Find first element that key is before, using predicate function
template<class T, class K>
T* roUpperBound(T* ary, roSize count, const K& key, bool(*less)(const K&, const T&))
{
	T* end = ary + count;
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(!less(key, *mid))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary == end ? NULL : ary;
}

template<class T>
void roInsertionSort(T* begin, T* end)
{
	roSize count = end - begin;
	for(roSize i=1; i<count; ++i) {
		for(roSize j=i; j>0 && begin[j] < begin[j-1]; --j)
			roSwap(begin[j], begin[j-1]);
	}
}

template<class T>
void roSelectionSort(T* begin, T* end)
{
	roSize count = end - begin;
	for(roSize i=0; i<count; ++i) {
		roSize k=i;
		for(roSize j=i+1; j<count; ++j)
			if(begin[j] < begin[k]) k = j;
		roSwap(begin[i], begin[k]);
	}
}

#endif	// __roAlgorithm_h__
