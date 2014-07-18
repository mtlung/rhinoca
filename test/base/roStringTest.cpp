#include "pch.h"
#include "../../roar/base/roString.h"

using namespace ro;

struct StringTest {};
struct RangedStringTest {};

TEST_FIXTURE(StringTest, compare)
{
	CHECK(String("AB") == String("AB"));

	CHECK(String("A") < String("B"));
	CHECK(String("A") < String("AB"));
	CHECK(String("AB") < String("AC"));

	CHECK(String("B") > String("A"));
	CHECK(String("AB") > String("A"));
	CHECK(String("AC") > String("AB"));
}

TEST_FIXTURE(RangedStringTest, compare)
{
	CHECK(RangedString("AB") == RangedString("AB"));

	CHECK(RangedString("A") < RangedString("B"));
	CHECK(RangedString("A") < RangedString("AB"));
	CHECK(RangedString("AB") < RangedString("AC"));

	CHECK(RangedString("B") > RangedString("A"));
	CHECK(RangedString("AB") > RangedString("A"));
	CHECK(RangedString("AC") > RangedString("AB"));
}
