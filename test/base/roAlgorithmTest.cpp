#include "pch.h"
#include "../../roar/base/roAlgorithm.h"

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
