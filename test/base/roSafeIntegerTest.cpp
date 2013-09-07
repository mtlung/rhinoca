#include "pch.h"
#include "../../roar/base/roSafeInteger.h"

using namespace ro;

class SafeIntegerTest {};

TEST_FIXTURE(SafeIntegerTest, addition_8bits)
{
	{	roUint8 a, b, c;

		a = 0, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(1, c);
		a = 255, b = 1;		CHECK(!roSafeAdd(a, b, c));

		a = 255, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(255, c);
		a = 0, b = 255;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(255, c);

		a = 255, b = 1;		CHECK(!roSafeAdd(a, b, c));
		a = 1, b = 255;		CHECK(!roSafeAdd(a, b, c));
		a = 128, b = 128;	CHECK(!roSafeAdd(a, b, c));
		a = 255, b = 255;	CHECK(!roSafeAdd(a, b, c));
	}

	{	roInt8 a, b, c;

		a = 0, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0, c);

		a = 127, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(127, c);
		a = 0, b = 127;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(127, c);

		a = 127, b = 1;		CHECK(!roSafeAdd(a, b, c));
		a = 1, b = 127;		CHECK(!roSafeAdd(a, b, c));
		a = 64, b = 64;		CHECK(!roSafeAdd(a, b, c));
		a = 127, b = 127;	CHECK(!roSafeAdd(a, b, c));

		a = -128, b = 0;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(-128, c);
		a = 0, b = -128;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(-128, c);

		a = -127, b = -1;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(-128, c);
		a = -1, b = -127;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(-128, c);
		a = -64, b = -64;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(-128, c);

		a = -128, b = -1;	CHECK(!roSafeAdd(a, b, c));
		a = -1, b = -128;	CHECK(!roSafeAdd(a, b, c));
		a = -65, b = -64;	CHECK(!roSafeAdd(a, b, c));
		a = -128, b = -127;	CHECK(!roSafeAdd(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, addition_32bits)
{
	{	roUint32 a, b, c;
		roUint32 max = ro::TypeOf<roUint32>::valueMax();

		a = 0, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0u, c);
		a = 0, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(1u, c);
		a = max, b = 1;		CHECK(!roSafeAdd(a, b, c));

		a = max, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(max, c);
		a = 0, b = max;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(max, c);

		a = max, b = 1;		CHECK(!roSafeAdd(a, b, c));
		a = 1, b = max;		CHECK(!roSafeAdd(a, b, c));
		a = max, b = max;	CHECK(!roSafeAdd(a, b, c));
	}

	{	roInt32 a, b, c;
		roInt32 min = ro::TypeOf<roInt32>::valueMin();
		roInt32 max = ro::TypeOf<roInt32>::valueMax();

		a = 0, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = 1;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(0, c);

		a = max, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(max, c);
		a = 0, b = max;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(max, c);

		a = max, b = 1;		CHECK(!roSafeAdd(a, b, c));
		a = 1, b = max;		CHECK(!roSafeAdd(a, b, c));
		a = max, b = max;	CHECK(!roSafeAdd(a, b, c));

		a = min, b = 0;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(min, c);
		a = 0, b = min;		CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(min, c);

		a = min+1, b = -1;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(min, c);
		a = -1, b = min+1;	CHECK(roSafeAdd(a, b, c));	CHECK_EQUAL(min, c);

		a = min, b = -1;	CHECK(!roSafeAdd(a, b, c));
		a = -1, b = min;	CHECK(!roSafeAdd(a, b, c));
		a = min, b = min;	CHECK(!roSafeAdd(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, subtraction_8bits)
{
	{	roUint8 a, b, c;

		a = 0, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(1, c);
		a = 1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);

		a = 1, b = 2;		CHECK(!roSafeSub(a, b, c));
	}

	{	roInt8 a, b, c;

		a = 0, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-2, c);
		a = 1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = -1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(2, c);
		a = 1, b = 127;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-126, c);
		a = 0, b = 127;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-127, c);
		a = -1, b = 127;	CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-128, c);

		a = -2, b = 127;	CHECK(!roSafeSub(a, b, c));

		a = -128, b = 0;	CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-128, c);
		a = -128, b = 1;	CHECK(!roSafeSub(a, b, c));
		a = -128, b = 64;	CHECK(!roSafeSub(a, b, c));
		a = -128, b = 127;	CHECK(!roSafeSub(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, subtraction_32bits)
{
	{	roUint32 a, b, c;

		a = 0, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0u, c);
		a = 1, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(1u, c);
		a = 1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0u, c);

		a = 1, b = 2;		CHECK(!roSafeSub(a, b, c));
	}

	{	roInt32 a, b, c;
		roInt32 min = ro::TypeOf<roInt32>::valueMin();
		roInt32 max = ro::TypeOf<roInt32>::valueMax();

		a = 0, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(-2, c);
		a = 1, b = 1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = -1;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(2, c);
		a = 1, b = max;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(min+2, c);
		a = 0, b = max;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(min+1, c);
		a = -1, b = max;	CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(min, c);

		a = -2, b = max;	CHECK(!roSafeSub(a, b, c));

		a = min, b = 0;		CHECK(roSafeSub(a, b, c));	CHECK_EQUAL(min, c);
		a = min, b = 1;		CHECK(!roSafeSub(a, b, c));
		a = min, b = max/2;	CHECK(!roSafeSub(a, b, c));
		a = min, b = max;	CHECK(!roSafeSub(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, multiplication_8bits)
{
	{	roUint8 a, b, c;

		a = 0, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1, c);
		a = 1, b = 255;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(255, c);
		a = 15, b = 16;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(240, c);

		a = 2, b = 255;		CHECK(!roSafeMul(a, b, c));
		a = 16, b = 16;		CHECK(!roSafeMul(a, b, c));	// 16 * 16 = 256
	}

	{	roInt8 a, b, c;

		// Pos * Pos
		a = 0, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1, c);
		a = 1, b = 127;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(127, c);
		a = 127, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(127, c);
		a = 2, b = 63;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(126, c);
		a = 63, b = 2;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(126, c);

		a = 2, b = 64;		CHECK(!roSafeMul(a, b, c));
		a = 64, b = 2;		CHECK(!roSafeMul(a, b, c));
		a = 12, b = 12;		CHECK(!roSafeMul(a, b, c));	// 12 * 12 = 144

		// Neg * Neg
		a = -1, b = -1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = -127;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(127, c);
		a = -127, b = -1;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(127, c);
		a = -2, b = -63;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(126, c);
		a = -63, b = -2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(126, c);

		a = -2, b = -64;	CHECK(!roSafeMul(a, b, c));
		a = -64, b = -2;	CHECK(!roSafeMul(a, b, c));
		a = -12, b = -12;	CHECK(!roSafeMul(a, b, c));	// -12 * -12 = 144

		// Neg * Pos
		a = -1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-1, c);
		a = -1, b = 127;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-127, c);
		a = -128, b = 1;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-128, c);
		a = -2, b = 63;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-126, c);
		a = -63, b = 2;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-126, c);
		a = -2, b = 64;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-128, c);
		a = -64, b = 2;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-128, c);

		a = -2, b = 65;		CHECK(!roSafeMul(a, b, c));
		a = -65, b = 2;		CHECK(!roSafeMul(a, b, c));
		a = -12, b = 12;	CHECK(!roSafeMul(a, b, c));	// -12 * 12 = -144
	}
}

TEST_FIXTURE(SafeIntegerTest, multiplication_32bits)
{
	{	roUint32 a, b, c;
		roUint32 max = ro::TypeOf<roUint32>::valueMax();

		a = 0, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0u, c);
		a = 1, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0u, c);
		a = 0, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0u, c);
		a = 1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1u, c);
		a = 1, b = max;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max, c);
		a = 65535, b = 65536;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(4294901760u, c);

		a = 2, b = max;		CHECK(!roSafeMul(a, b, c));
		a = 65536, b = 65536;	CHECK(!roSafeMul(a, b, c));
	}

	{	roInt32 a, b, c;
		roInt32 min = ro::TypeOf<roInt32>::valueMin();
		roInt32 max = ro::TypeOf<roInt32>::valueMax();
		roInt32 sq = 46340;

		// Pos * Pos
		a = 0, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 0;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 0, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(0, c);
		a = 1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1, c);
		a = 1, b = max;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max, c);
		a = max, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max, c);
		a = 2, b = max/2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max-1, c);
		a = max/2, b = 2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max-1, c);
		a = sq, b = sq;		CHECK(roSafeMul(a, b, c));

		a = 2, b = max/2+1;	CHECK(!roSafeMul(a, b, c));
		a = max/2+1, b = 2;	CHECK(!roSafeMul(a, b, c));
		a = sq+1, b = sq+1;	CHECK(!roSafeMul(a, b, c));

		// Neg * Neg
		a = -1, b = -1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(1, c);
		a = -1, b = -max;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max, c);
		a = -max, b = -1;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max, c);
		a = -2, b = -max/2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max-1, c);
		a = -max/2, b = -2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(max-1, c);
		a = -sq, b = -sq;	CHECK(roSafeMul(a, b, c));

		a = -2, b = -max/2-1;	CHECK(!roSafeMul(a, b, c));
		a = -max/2-1, b = -2;	CHECK(!roSafeMul(a, b, c));
		a = -sq-1, b = -sq-1;	CHECK(!roSafeMul(a, b, c));

		// Neg * Pos
		a = -1, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-1, c);
		a = -1, b = max;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-max, c);
		a = min, b = 1;		CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(min, c);
		a = -2, b = max/2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-max+1, c);
		a = -max/2, b = 2;	CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(-max+1, c);
		a = -2, b = max/2+1;CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(min, c);
		a = -max/2-1, b = 2;CHECK(roSafeMul(a, b, c));	CHECK_EQUAL(min, c);
		a = -sq, b = sq;	CHECK(roSafeMul(a, b, c));

		a = -2, b = max/2+2;CHECK(!roSafeMul(a, b, c));
		a = -max/2-2, b = 2;CHECK(!roSafeMul(a, b, c));
		a = -sq-1, b = sq+1;CHECK(!roSafeMul(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, division)
{
	{	roUint8 a, b, c;

		a = 10, b = 2;		CHECK(roSafeDiv(a, b, c));	CHECK_EQUAL(5, c);
		a = 10, b = 1;		CHECK(roSafeDiv(a, b, c));	CHECK_EQUAL(10, c);
		a = 0, b = 1;		CHECK(roSafeDiv(a, b, c));	CHECK_EQUAL(0, c);
		a = 10, b = 0;		CHECK(!roSafeDiv(a, b, c));
		a = 0, b = 0;		CHECK(!roSafeDiv(a, b, c));
	}

	{	roInt8 a, b, c;

		a = 10, b = 2;		CHECK(roSafeDiv(a, b, c));	CHECK_EQUAL(5, c);
		a = 10, b = 0;		CHECK(!roSafeDiv(a, b, c));

		a = -1, b = -1;		CHECK(roSafeDiv(a, b, c));	CHECK_EQUAL(1, c);
		a = -128, b = -1;	CHECK(!roSafeDiv(a, b, c));
	}
}

TEST_FIXTURE(SafeIntegerTest, abs)
{
	{	roUint8 a, b;

		a = 0;		CHECK(roSafeAbs(a, b));	CHECK_EQUAL(0, b);
		a = 1;		CHECK(roSafeAbs(a, b));	CHECK_EQUAL(1, b);
		a = 255;	CHECK(roSafeAbs(a, b));	CHECK_EQUAL(255, b);
	}

	{	roInt8 a, b;

		a = -128;	CHECK(!roSafeAbs(a, b));
		a = -127;	CHECK(roSafeAbs(a, b));	CHECK_EQUAL(127, b);
		a = -1;		CHECK(roSafeAbs(a, b));	CHECK_EQUAL(1, b);
		a = 0;		CHECK(roSafeAbs(a, b));	CHECK_EQUAL(0, b);
		a = 1;		CHECK(roSafeAbs(a, b));	CHECK_EQUAL(1, b);
		a = 127;	CHECK(roSafeAbs(a, b));	CHECK_EQUAL(127, b);
	}
}

TEST_FIXTURE(SafeIntegerTest, negate)
{
	{	roUint8 a, b;

		a = 0;		CHECK(!roSafeNeg(a, b));
		a = 1;		CHECK(!roSafeNeg(a, b));
		a = 255;	CHECK(!roSafeNeg(a, b));
	}

	{	roInt8 a, b;

		a = -128;	CHECK(!roSafeNeg(a, b));
		a = -127;	CHECK(roSafeNeg(a, b));	CHECK_EQUAL(127, b);
		a = -1;		CHECK(roSafeNeg(a, b));	CHECK_EQUAL(1, b);
		a = 0;		CHECK(roSafeNeg(a, b));	CHECK_EQUAL(0, b);
		a = 1;		CHECK(roSafeNeg(a, b));	CHECK_EQUAL(-1, b);
		a = 127;	CHECK(roSafeNeg(a, b));	CHECK_EQUAL(-127, b);
	}
}

TEST_FIXTURE(SafeIntegerTest, comparison_equality)
{
	{	// Demonstration of the problem
		roInt64 a = -1;
		roUint64 b = ro::TypeOf<roUint64>::valueMax();
		CHECK((roUint64)a == b);	// Obviously incorrect
	}

	{	roInt64 a = 2;
		roUint64 b = 3;
		CHECK(!roIsEqual(a, b));
		CHECK(!roIsEqual(b, a));
		CHECK(roIsNotEqual(a, b));
		CHECK(roIsNotEqual(b, a));
	}

	{	roInt64 a = -1;
		roUint64 b = ro::TypeOf<roUint64>::valueMax();
		CHECK(!roIsEqual(a, b));
		CHECK(!roIsEqual(b, a));
		CHECK(roIsNotEqual(a, b));
		CHECK(roIsNotEqual(b, a));
	}
}

TEST_FIXTURE(SafeIntegerTest, comparison_greater)
{
	{	// This simply test demonstrate the problem of signed/unsigned comparison
		roInt64 a = -1;
		roUint64 b = 1;
		CHECK((roUint64)a > b);	// As you see, -1 > 1, which doesn't make sense
		CHECK(!((roUint64)a < b));

		CHECK(!roIsGreater(a, b));
		CHECK(roIsGreater(b, a));
	}

	{	// Problem should get worse if the signed type having a even smaller size
		roInt32 a = -1;
		roUint64 b = 1;
		CHECK(a > b);	// As you see, -1 > 1, which doesn't make sense
		CHECK(!(a < b));

		CHECK(!roIsGreater(a, b));
		CHECK(roIsGreater(b, a));
	}

	{	// No problem if signed type having a larger size
		roInt64 a = -1;
		roUint32 b = 1;
		CHECK(!(a > b));
		CHECK(a < b);

		CHECK(!roIsGreater(a, b));
		CHECK(roIsGreater(b, a));
	}

	{	// Interesting that int8 and 16 didn't have the problem
		roInt8 a = -1;
		roUint16 b = 1;
		CHECK(!(a > b));
		CHECK(a < b);

		CHECK(!roIsGreater(a, b));
		CHECK(roIsGreater(b, a));
	}

	{	// Same bit size
		CHECK(roIsGreater(1, 0));
		CHECK(roIsGreater(1u, 0));
		CHECK(roIsGreater(1, 0u));
		CHECK(roIsGreater(1u, 0u));

		CHECK(!roIsGreater(0, 0));
		CHECK(!roIsGreater(0u, 0));
		CHECK(!roIsGreater(0, 0u));
		CHECK(!roIsGreater(0u, 0u));

		CHECK(roIsGreater(0, -1));
		CHECK(roIsGreater(0u, -1));
		CHECK(roIsGreater(-1, -2));

		CHECK(!roIsGreater(-1, 0));
		CHECK(!roIsGreater(-1, 0u));
		CHECK(!roIsGreater(-2, -1));
	}

	{	// Greater equal
		CHECK(roIsGreaterEqual(0, 0));
		CHECK(roIsGreaterEqual(1, 1));
		CHECK(roIsGreaterEqual(1u, 1));
		CHECK(roIsGreaterEqual(1, 1u));
		CHECK(roIsGreaterEqual(1u, 1u));
		CHECK(roIsGreaterEqual(-1, -1));
		CHECK(roIsGreaterEqual(1, -1));
		CHECK(roIsGreaterEqual(1u, -1));

		CHECK(!roIsGreaterEqual(0, 1));
		CHECK(!roIsGreaterEqual(0u, 1));
		CHECK(!roIsGreaterEqual(0, 1u));
		CHECK(!roIsGreaterEqual(0u, 1u));
		CHECK(!roIsGreaterEqual(-2, -1));
		CHECK(!roIsGreaterEqual(-1, 1));
		CHECK(!roIsGreaterEqual(-1, 1u));
	}
}

TEST_FIXTURE(SafeIntegerTest, comparison_less)
{
	{	CHECK(roIsLess(0, 1));
		CHECK(roIsLess(0, 1u));
		CHECK(roIsLess(0u, 1));
		CHECK(roIsLess(0u, 1u));

		CHECK(!roIsLess(1, 0));
		CHECK(!roIsLess(1u, 0));
		CHECK(!roIsLess(1, 0u));
		CHECK(!roIsLess(1u, 0u));

		CHECK(!roIsLess(0, 0));
		CHECK(!roIsLess(0u, 0));
		CHECK(!roIsLess(0, 0u));
		CHECK(!roIsLess(0u, 0u));

		CHECK(!roIsLess(0, -1));
		CHECK(!roIsLess(0u, -1));
		CHECK(!roIsLess(-1, -2));

		CHECK(roIsLess(-1, 0));
		CHECK(roIsLess(-1, 0u));
		CHECK(roIsLess(-2, -1));
	}

	{	// Less equal
		CHECK(roIsLessEqual(0, 0));
		CHECK(roIsLessEqual(1, 1));
		CHECK(roIsLessEqual(1u, 1));
		CHECK(roIsLessEqual(1, 1u));
		CHECK(roIsLessEqual(1u, 1u));
		CHECK(roIsLessEqual(-1, -1));
		CHECK(roIsLessEqual(-1, 1));
		CHECK(roIsLessEqual(-1, 1u));

		CHECK(!roIsLessEqual(1, 0));
		CHECK(!roIsLessEqual(1, 0u));
		CHECK(!roIsLessEqual(1u, 0));
		CHECK(!roIsLessEqual(1u, 0u));
		CHECK(!roIsLessEqual(-1, -2));
		CHECK(!roIsLessEqual(1, -1));
		CHECK(!roIsLessEqual(1u, -1));
	}
}