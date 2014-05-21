#include "pch.h"
#include "../../roar/base/roAlgorithm.h"
#include "../../roar/base/roArray.h"
#include "../../roar/math/roRandom.h"

using namespace ro;

class AlgorithmTest {};

TEST_FIXTURE(AlgorithmTest, lowerBound)
{
	{	int a[] = { 1 };
		CHECK(				!roLowerBound(a, roCountof(a), a[0] + 1));
		CHECK_EQUAL(a[0],	*roLowerBound(a, roCountof(a), a[0]));
		CHECK_EQUAL(a[0],	*roLowerBound(a, roCountof(a), a[0] - 1));

		CHECK(				!roUpperBound(a, roCountof(a), a[0] + 1));
		CHECK(				!roUpperBound(a, roCountof(a), a[0]));
		CHECK_EQUAL(a[0],	*roUpperBound(a, roCountof(a), a[0] - 1));
	}

	{	int a[] = { 0, 1, 10, 11 };
		CHECK_EQUAL(10, *roLowerBound(a, roCountof(a), 5));
		CHECK_EQUAL(10, *roUpperBound(a, roCountof(a), 5));
	}

	{	int a[] = { 0, 1, 5, 5, 10, 11 };
		CHECK_EQUAL(2, roLowerBound(a, roCountof(a), 5) - a);
		CHECK_EQUAL(4, roUpperBound(a, roCountof(a), 5) - a);
	}
}

TEST_FIXTURE(AlgorithmTest, partition)
{
	struct Pred { static bool lessThan5(const int& x) {
		return x < 5;
	}};
	
	{	int v[]			= { 7, 1, 8, 4, 5, 0, 2, 2, 3 };
		int expected[]	= { 1, 4, 0, 2, 2, 3, 7, 5, 8 };
		CHECK_EQUAL(7, *roPartition(v, v + roCountof(v), Pred::lessThan5));
		CHECK(roEqual(v, v + roCountof(v), expected));
	}

	{	int v[]			= { 10, 1, 9, 2, 0, 5, 7, 3, 4, 6, 8 };
		int expected[]	= { 1, 2, 0, 3, 4, 5, 7, 10, 9, 6, 8 };
		CHECK_EQUAL(5, *roPartition(v, v + roCountof(v), Pred::lessThan5));
		CHECK(roEqual(v, v + roCountof(v), expected));
	}

	{	int v[]			= { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
		int expected[]	= { 4, 3, 2, 1, 0, 9, 8, 7, 6, 5 };
		CHECK_EQUAL(9, *roPartition(v, v + roCountof(v), Pred::lessThan5));
		CHECK(roEqual(v, v + roCountof(v), expected));
	}

	{	int v[]			= { 4, 4, 4, 4, 4 };
		CHECK_EQUAL(v + roCountof(v), roPartition(v, v + roCountof(v), Pred::lessThan5));
	}

	{	int v[]			= { 6, 6, 6, 6, 6 };
		CHECK_EQUAL(v, roPartition(v, v + roCountof(v), Pred::lessThan5));
	}

	{	int v[]			= { 1, 6, 6, 6, 6 };
		CHECK_EQUAL(v + 1, roPartition(v, v + roCountof(v), Pred::lessThan5));
	}
}

TEST_FIXTURE(AlgorithmTest, sort)
{
	struct Check { static bool isSorted(const Array<int>& a) {
		for(roSize i=1; i<a.size(); ++i)
			if(a[i] < a[i-1]) return false;
		return true;
	}};

	Array<int> accending, decending, random;

	for(int i=0; i<10; ++i) {
		accending.pushBack(i);
		decending.insert(0, i);
		random.insert(roRandBeginEnd(roSize(0), random.size()), i);
	}

	{	// Insertion sort
		Array<int> v;
		roInsertionSort(v.begin(), v.end());

		v = accending;
		roInsertionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = decending;
		roInsertionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = random;
		roInsertionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));
	}

	{	// Selection sort
		Array<int> v;
		roSelectionSort(v.begin(), v.end());

		v = accending;
		roSelectionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = decending;
		roSelectionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = random;
		roSelectionSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));
	}

	{	// Heap sort
		Array<int> v;
		roHeapSort(v.begin(), v.end());

		v = accending;
		roHeapSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = decending;
		roHeapSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = random;
		roHeapSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));
	}

	{	// Quick sort
		Array<int> v;

		v = accending;
		roQuickSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = decending;
		roQuickSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));

		v = random;
		roQuickSort(v.begin(), v.end());
		CHECK(Check::isSorted(v));
	}
}
