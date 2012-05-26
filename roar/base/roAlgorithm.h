#ifndef __roAlgorithm_h__
#define __roAlgorithm_h__

#include "../platform/roCompiler.h"

// Searching
template<class T>			T*		roArrayFind	(T* begin, T* end, const T& key);
template<class T>			T*		roArrayFind	(T* begin, roSize count, const T& key);
template<class T, class K>	T*		roArrayFind	(T* begin, roSize count, const K& key, bool(*equal)(const T&, const K&));

template<class T>			T*		roLowerBound(T* ary, roSize count, const T& key);
template<class T, class K>	T*		roLowerBound(T* ary, roSize count, const K& key, bool(*less)(const T&, const K&));
template<class T>			T*		roUpperBound(T* ary, roSize count, const T& key);
template<class T, class K>	T*		roUpperBound(T* ary, roSize count, const K& key, bool(*less)(const K&, const T&));

//
template<class T>			bool	roEqual(T* begin, T* end, T* begin2);
template<class T, class P>	bool	roEqual(T* begin, T* end, T* begin2, P p);

//
template<class T>			T*		roPartition(T* begin, T* end, bool(*pred)(const T&));

// Sorting
template<class T>			void	roInsertionSort(T* begin, T* end);	/// Stable, O(1) space, O(n2) compare and swaps, adaptive
template<class T, class P>	void	roInsertionSort(T* begin, T* end, P p);
template<class T>			void	roSelectionSort(T* begin, T* end);	/// Not stable, O(1) space, O(n2) compare, O(n) swaps, non-adaptive
template<class T, class P>	void	roSelectionSort(T* begin, T* end, P p);
template<class T>			void	roHeapSort(T* begin, T* end);
template<class T, class P>	void	roHeapSort(T* begin, T* end, P p);
template<class T>			void	roQuickSort(T* begin, T* end);		/// Not stable, O(1) space
template<class T, class P>	void	roQuickSort(T* begin, T* end, P p);

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
bool roEqual(T* begin, T* end, T* begin2)
{
	for(T* i=begin; i<end; ++i, ++begin2)
		if(!(*i == *begin2))
			return false;
	return true;
}

template<class T, class Pred>
bool roEqual(T* begin, T* end, T* begin2, Pred pred)
{
	for(T* i=begin; i<end; ++i, ++begin2)
		if(!pred(*i, *begin2))
			return false;
	return true;
}

/// Parition the sequence into 2 such that left part fulfill pred, and the second do not
/// Return the first element which do not fulfill pred
template<class T>
T* roPartition(T* begin, T* end, bool(*pred)(const T&))
{
	T* p = begin;
	for(T* i=begin; i<end; ++i) {
		if(pred(*i))
			roSwap(*i, *p), ++p;
	}
	return p;
}

template<class T, class Pred>
void roInsertionSort(T* begin, T* end, Pred pred)
{
	// For each element, find (towards the font) correct position to insert
	for(T* i=begin; i<end; ++i)
		for(T* j=i; j>begin && pred(*j, *(j-1)); --j)
			roSwap(*j, *(j-1));
}

template<class T>
void roInsertionSort(T* begin, T* end)
{
	struct _Less { bool operator()(const T& a, const T& b) {
		return a < b;
	}};
	roInsertionSort(begin, end, _Less());
}

template<class T, class Pred>
void roSelectionSort(T* begin, T* end, Pred pred)
{
	// Select the smallest in the whole list, and swap it to the font
	// Advance the font by one
	for(T* i=begin; i<end; ++i) {
		T* k=i;
		for(T* j=i+1; j<end; ++j)
			if(pred(*j, *k)) k = j;
		roSwap(*i, *k);
	}
}

template<class T>
void roSelectionSort(T* begin, T* end)
{
	struct _Less { bool operator()(const T& a, const T& b) {
		return a < b;
	}};
	roSelectionSort(begin, end, _Less());
}

template<class T, class Pred>
void roHeapSort(T* begin, T* end, Pred pred)
{
	struct Local {
	// A heap is where the value of the parent bigger than it's 2 children
	static void makeHeap(T* b, T* e, Pred pred) {
		for(T* i=b+1; i<e; ++i) {
			for(T* mov=i;;) {
				T* parent = b + (mov - b - 1) / 2;
				if(pred(*parent, *mov)) {
					roSwap(*mov, *parent);
					mov = parent;
				}
				else
					break;
			}
		}
	}

	static void shiftDown(T* b, T* e, T* i, Pred pred) {
		while(true) {
			T* l = b + (i - b) * 2 + 1;	// Left child
			T* r = l + 1;				// Right child
			if(l >= e)
				return;

			T* pSwap = i;
			if(pred(*pSwap, *l))			pSwap = l;
			if(r < e && pred(*pSwap, *r))	pSwap = r;

			if(pSwap == i)
				return;

			roSwap(*i, *pSwap);
			i = pSwap;
		}
	}
	};

	// This is the shift up version
//	Local::makeHeap(begin, end, pred);

	// This is the shitf down version
	if(begin >= end) return;
	for(T* i=begin + (end - begin - 2) / 2; i >= begin; --i)
		Local::shiftDown(begin, end, i, pred);

	// Put the front (where it shold be the largest) to the end
	// and re-balance the heap
	while(begin < end) {
		--end;
		roSwap(*begin, *end);
		Local::shiftDown(begin, end, begin, pred);
	}
}

template<class T>
void roHeapSort(T* begin, T* end)
{
	struct _Less { bool operator()(const T& a, const T& b) {
		return a < b;
	}};
	roHeapSort(begin, end, _Less());
}

template<class T, class Pred>
void roQuickSort(T* begin, T* end, Pred pred)
{
	if(end - begin < 5)
		return roSelectionSort(begin, end, pred);

	struct Local { static T* partition(T* b, T* e, Pred pred) {
		roAssert(e - b > 2);
		T* p = b;
		T* pivot = b + (e - b) / 2;
		T* l = e - 1;	// The last element

		if(pred(*pivot, *b)) roSwap(*pivot, *b);
		if(pred(*l, *pivot)) roSwap(*l, *pivot);
		if(pred(*pivot, *b)) roSwap(*pivot, *b);

		roSwap(*pivot, *l);	// Move pivot to end
		pivot = l;
		for(T* i=b; i<l; ++i) {
			if(pred(*i, *pivot))
				roSwap(*i, *p), ++p;
		}
		roSwap(*p, *l); // Move pivot to its final place
		return p;
	}};

	T* pivot = Local::partition(begin, end, pred);
	roQuickSort(begin, pivot, pred);
	roQuickSort(pivot+1, end, pred);
}

template<class T>
void roQuickSort(T* begin, T* end)
{
	struct _Less { bool operator()(const T& a, const T& b) {
		return a < b;
	}};
	roQuickSort(begin, end, _Less());
}

#endif	// __roAlgorithm_h__
