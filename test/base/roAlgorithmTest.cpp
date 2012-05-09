#include "pch.h"
#include "../../roar/base/roAlgorithm.h"
#include "../../roar/base/roArray.h"

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

TEST_FIXTURE(AlgorithmTest, sort)
{
	struct Check { static bool isSorted(const Array<int>& a) {
		for(roSize i=1; i<a.size(); ++i)
			if(a[i] < a[i-1]) return false;
		return true;
	}};

	Array<int> accending, decending, random;

	for(roSize i=0; i<10; ++i) {
		accending.pushBack(i);
		decending.insert(0, i);
		random.insert(rand() % (random.size()+1), i);
	}

	{	// Insertion sort
		Array<int> v;

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
}
