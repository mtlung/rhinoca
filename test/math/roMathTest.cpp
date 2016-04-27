#include "pch.h"
#include "../../roar/math/roMath.h"

using namespace ro;

struct MathTest {};

TEST_FIXTURE(MathTest, power10)
{
	CHECK_EQUAL(0, roPow10d(-309));
	CHECK_EQUAL(1e-308, roPow10d(-308));
	CHECK_EQUAL(0.1, roPow10d(-1));
	CHECK_EQUAL(1, roPow10d(0));
	CHECK_EQUAL(10, roPow10d(1));
	CHECK_EQUAL(1e308, roPow10d(308));
	CHECK_EQUAL(INFINITY, roPow10d(309));
}

TEST_FIXTURE(MathTest, rounding)
{
	static const double epsilon = 0.00001;

	CHECK_CLOSE(123.0, roRoundTo(123.456, 0), epsilon);
	CHECK_CLOSE(-123.0, roRoundTo(-123.456, 0), epsilon);

	CHECK_CLOSE(124.0, roRoundTo(123.555, 0), epsilon);
	CHECK_CLOSE(-124.0, roRoundTo(-123.555, 0), epsilon);

	CHECK_CLOSE(123.5, roRoundTo(123.456, 1), epsilon);
	CHECK_CLOSE(-123.5, roRoundTo(-123.456, 1), epsilon);

	CHECK_CLOSE(123.4, roRoundTo(123.444, 1), epsilon);
	CHECK_CLOSE(-123.4, roRoundTo(-123.444, 1), epsilon);
}
