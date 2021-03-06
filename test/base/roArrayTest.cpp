#include "pch.h"
#include "../../roar/base/roArray.h"

using namespace ro;

struct TinyArrayTest {};

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

	v.reserve(2, false);
	CHECK_EQUAL(3u, v.capacity());	// Don't force reserve to change size, so it keep capacity as 3
	v.reserve(2, true);
	CHECK_EQUAL(2u, v.capacity());	// Force capacity to be 2

	v.clear();
	CHECK_EQUAL(0u, v.size());
	CHECK_EQUAL(2u, v.capacity());

	v.condense();
	CHECK_EQUAL(1u, v.capacity());	// The smallest reserved size is the pre-allocated size
}

TEST_FIXTURE(TinyArrayTest, insert)
{
	{	TinyArray<int, 1> v;

		// Insert at beginning
		v.insert(0, 3);
		CHECK_EQUAL(3, v[0]);

		v.insert(0, 1);
		CHECK_EQUAL(1, v[0]);

		// Insert in the middle
		v.insert(1, 2);
		CHECK_EQUAL(2, v[1]);

		// Insert at the end
		v.insert(v.size(), 4);
		CHECK_EQUAL(4, v.back());

		// Insert unique
		v.insertUnique(0, 4);
		CHECK_EQUAL(4, v.size());
		v.insertUnique(0, 5);
		CHECK_EQUAL(5, v.size());
	}

	{	Array<int> v;

		// Insert nothing
		v.insert(0, (int*)NULL, (int*)NULL);

		// Insert range at beginning
		int r1[] = { 5, 6 };
		v.insert(0, r1, r1 + roCountof(r1));
		CHECK_EQUAL(5, v[0]); CHECK_EQUAL(6, v[1]);

		int r2[] = { 1, 2 };
		v.insert(0, r2, r2 + roCountof(r2));
		CHECK_EQUAL(1, v[0]); CHECK_EQUAL(2, v[1]);

		// Insert range in the middle
		int r3[] = { 3, 4 };
		v.insert(2, r3, r3 + roCountof(r3));
		CHECK_EQUAL(3, v[2]); CHECK_EQUAL(4, v[3]);

		// Insert range at the end
		int r4[] = { 7, 8 };
		v.insert(v.size(), r4, r4 + roCountof(r4));
		CHECK_EQUAL(7, v[6]); CHECK_EQUAL(8, v[7]);
	}
}

TEST_FIXTURE(TinyArrayTest, arrayOfArray)
{
	TinyArray<TinyArray<int, 2>, 1> v;
	v.incSize(1);

	v[0].pushBack(1);
	v[0].pushBack(2);	// A static to dynamic transition should occurred

	v.incSize(1);

	CHECK_EQUAL(1, v[0][0]);
	CHECK_EQUAL(2, v[0][1]);
}

TEST_FIXTURE(TinyArrayTest, initList)
{
	TinyArray<int, 2> v = { 1 };
	CHECK_EQUAL(1, v.size());
	CHECK_EQUAL(1, v[0]);

	v = {};
	CHECK_EQUAL(0, v.size());

	v = { 1, 2, 3 };
	CHECK_EQUAL(3, v.size());
	CHECK_EQUAL(1, v[0]);
	CHECK_EQUAL(2, v[1]);
	CHECK_EQUAL(3, v[2]);
}

namespace {

struct MyClass {
	TinyArray<int, 1> v;
};

}	// namespace

TEST_FIXTURE(TinyArrayTest, arrayOfClassWithArray)
{
	TinyArray<MyClass, 1> v;
	v.incSize(1);

	v[0].v.pushBack(1);
	v[0].v.pushBack(2);

	v.incSize(1);

	CHECK_EQUAL(1, v[0].v[0]);
	CHECK_EQUAL(2, v[0].v[1]);
}

struct SizeLimitedArrayTest {};

TEST_FIXTURE(SizeLimitedArrayTest, basic)
{
	SizeLimitedArray<Array<int> > v;
	CHECK_EQUAL(roStatus::size_limit_reached, v.incSize(1));

	v.maxSizeAllowed = 1;
	CHECK_EQUAL(roStatus::ok, v.pushBack(1));
	CHECK_EQUAL(roStatus::size_limit_reached, v.pushBack(2));

	v.maxSizeAllowed = 2;
	CHECK_EQUAL(roStatus::ok, v.pushBack(2));
}

