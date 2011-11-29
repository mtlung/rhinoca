#include "pch.h"
#include "../../roar/base/roArray.h"

using namespace ro;

class TinyArrayTest
{
public:
};

TEST_FIXTURE(TinyArrayTest, empty)
{
	TinyArray<int, 1> v;
}

TEST_FIXTURE(TinyArrayTest, basic)
{
	TinyArray<int, 1> v;

	CHECK_EQUAL(0u, v.size());
	CHECK_EQUAL(1u, v.capacity());

	v.resize(1, 123);
	CHECK_EQUAL(123, v[0]);
	CHECK_EQUAL(1u, v.size());
	CHECK_EQUAL(1u, v.capacity());

	v.resize(2, 456);
	CHECK_EQUAL(123, v[0]);
	CHECK_EQUAL(456, v[1]);
	CHECK_EQUAL(2u, v.size());
	CHECK_EQUAL(2u, v.capacity());

	v.resize(1, 789);
	CHECK_EQUAL(123, v[0]);	// Note it's 123 not 789
	CHECK_EQUAL(1u, v.size());
	CHECK_EQUAL(2u, v.capacity());

	v.reserve(3);
	CHECK_EQUAL(3u, v.capacity());

	v.reserve(2);
	CHECK_EQUAL(2u, v.capacity());

	v.clear();
	CHECK_EQUAL(0u, v.size());
	CHECK_EQUAL(2u, v.capacity());

	v.reserve(1);
}
