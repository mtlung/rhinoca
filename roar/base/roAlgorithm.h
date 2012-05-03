#include "../platform/roCompiler.h"

/// Find first element not before key, using operator<
template<class T>
T* roLowerBound(T* ary, roSize count, const T& key)
{
	for (; 0 < count; )
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(*mid < key)
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary;
}

/// Find first element not before key, using predicate function
template<class T, class K>
T* roLowerBound(T* ary, roSize count, const K& key, bool(*less)(const T&, const K&))
{
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(less(*mid, key))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary;
}

/// Find first element that key is before, using operator<
template<class T>
T* roUpperBound(T* ary, roSize count, const T& key)
{
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(!(key < *mid))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary;
}

/// Find first element that key is before, using predicate function
template<class T, class K>
T* roUpperBound(T* ary, roSize count, const K& key, bool(*less)(const K&, const T&))
{
	while(count)
	{	// Divide and conquer, find half that contains answer
		roSize count2 = count / 2;
		T* mid = ary + count2;

		if(!less(key, *mid))
			ary = ++mid, count -= count2 + 1;
		else
			count = count2;
	}
	return ary;
}
