#include "pch.h"
#include "../src/vector.h"
#include <math.h>
#include <vector>

class PreAllocVectorTest
{
public:
};

TEST_FIXTURE(PreAllocVectorTest, empty)
{
	PreAllocVector<int, 1> v;
}

TEST_FIXTURE(PreAllocVectorTest, basic)
{
	PreAllocVector<int, 1> v;

	CHECK_EQUAL(0, v.size());
	CHECK_EQUAL(1, v.capacity());

	v.resize(1, 123);
	CHECK_EQUAL(123, v[0]);
	CHECK_EQUAL(1, v.size());
	CHECK_EQUAL(1, v.capacity());

	v.resize(2, 456);
	CHECK_EQUAL(123, v[0]);
	CHECK_EQUAL(456, v[1]);
	CHECK_EQUAL(2, v.size());
	CHECK_EQUAL(2, v.capacity());

	v.resize(1, 789);
	CHECK_EQUAL(123, v[0]);	// Note it's 123 not 789
	CHECK_EQUAL(1, v.size());
	CHECK_EQUAL(2, v.capacity());

	v.reserve(3);
	CHECK_EQUAL(3, v.capacity());

	v.reserve(2);
	CHECK_EQUAL(2, v.capacity());

	v.clear();
	CHECK_EQUAL(0, v.size());
	CHECK_EQUAL(2, v.capacity());

	v.reserve(1);
}
