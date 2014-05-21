#include "pch.h"
#include "../../roar/base/roStringUtility.h"
#include "../../roar/base/roTypeOf.h"
#include <float.h>

struct StringUtilityTest {};

TEST_FIXTURE(StringUtilityTest, checkRange)
{
	// Success cases
	CHECK_EQUAL(0,		roStrToInt8("0",	1, 123));
	CHECK_EQUAL(0,		roStrToInt8("0  ",	3, 123));
	CHECK_EQUAL(0,		roStrToInt8("  0",	3, 123));
	CHECK_EQUAL(0,		roStrToInt8("  0  ",5, 123));
	CHECK_EQUAL(0,		roStrToInt8("-0",	2, 123));

	CHECK_EQUAL(0,		roStrToInt8("00",	2, 123));
	CHECK_EQUAL(0,		roStrToInt8(" 00",	3, 123));
	CHECK_EQUAL(0,		roStrToInt8("00 ",	3, 123));
	CHECK_EQUAL(0,		roStrToInt8(" 00 ",	4, 123));

	CHECK_EQUAL(23,		roStrToInt8("23",	2, 123));
	CHECK_EQUAL(23,		roStrToInt8("23 ",	3, 123));
	CHECK_EQUAL(23,		roStrToInt8("  23",	4, 123));
	CHECK_EQUAL(23,		roStrToInt8(" 23 ",	4, 123));
	CHECK_EQUAL(-23,	roStrToInt8("-23",	3, 123));
	CHECK_EQUAL(-23,	roStrToInt8("-23 ",	4, 123));
	CHECK_EQUAL(-23,	roStrToInt8(" -23",	4, 123));
	CHECK_EQUAL(-23,	roStrToInt8(" -23 ",5, 123));

	CHECK_EQUAL(-12,	roStrToInt8(" -123 ",4, 123));
	CHECK_EQUAL(-123,	roStrToInt8(" -123 ",5, 123));

	CHECK_EQUAL(206,	roStrToUint64("206",3, 123));

	// Fail cases
	CHECK_EQUAL(123,	roStrToInt8("",		0, 123));
	CHECK_EQUAL(123,	roStrToInt8(" 0",	0, 123));
	CHECK_EQUAL(123,	roStrToInt8(" 0",	1, 123));
	CHECK_EQUAL(123,	roStrToInt8(" 0 ",	1, 123));
	CHECK_EQUAL(123,	roStrToInt8("-",	1, 123));
	CHECK_EQUAL(123,	roStrToInt8(" -",	2, 123));
	CHECK_EQUAL(123,	roStrToInt8("- ",	2, 123));
	CHECK_EQUAL(123,	roStrToInt8(" - ",	3, 123));
	CHECK_EQUAL(123,	roStrToInt8("abc",	3, 123));
}

TEST_FIXTURE(StringUtilityTest, integer8)
{
	const roInt32 max8 = ro::TypeOf<roInt8>::valueMax();
	const roInt32 min8 = ro::TypeOf<roInt8>::valueMin();
	const roUint32 maxU8 = ro::TypeOf<roUint8>::valueMax();

	// Not a number
	CHECK_EQUAL(123,	roStrToInt8("abc", 123));

	// Test for signed
	CHECK_EQUAL(0,		roStrToInt8("  0", 123));
	CHECK_EQUAL(-123,	roStrToInt8(" -123", 0));
	CHECK_EQUAL(-123,	roStrToInt8("- 123", 0));

	CHECK_EQUAL(min8,	roStrToInt8(" -128", 0));
	CHECK_EQUAL(0,		roStrToInt8(" -2129", 0));
	CHECK_EQUAL(max8,	roStrToInt8("  127", 0));
	CHECK_EQUAL(0,		roStrToInt8("  128", 0));

	// Test for unsigned
	CHECK_EQUAL(0u,		roStrToUint8(" 0", 123));
	CHECK_EQUAL(123u,	roStrToUint8(" 123", 0));

	CHECK_EQUAL(0u,		roStrToUint8("-1", 0));
	CHECK_EQUAL(maxU8,	roStrToUint8(" 255", 0));
	CHECK_EQUAL(0u,		roStrToUint8(" 256", 0));
}

TEST_FIXTURE(StringUtilityTest, integer32)
{
	const roInt32 max32 = ro::TypeOf<roInt32>::valueMax();
	const roInt32 min32 = ro::TypeOf<roInt32>::valueMin();
	const roUint32 maxU32 = ro::TypeOf<roUint32>::valueMax();

	// Not a number
	CHECK_EQUAL(123,	roStrToInt32("abc", 123));

	// Test for signed
	CHECK_EQUAL(0,		roStrToInt32("  0", 123));
	CHECK_EQUAL(-123,	roStrToInt32(" -123", 0));
	CHECK_EQUAL(-123,	roStrToInt32("- 123", 0));

	CHECK_EQUAL(min32,	roStrToInt32(" -2147483648", 0));
	CHECK_EQUAL(0,		roStrToInt32(" -2147483649", 0));
	CHECK_EQUAL(max32,	roStrToInt32("  2147483647", 0));
	CHECK_EQUAL(0,		roStrToInt32("  2147483648", 0));

	// Test for unsigned
	CHECK_EQUAL(0u,		roStrToUint32(" 0", 123));
	CHECK_EQUAL(1234u,	roStrToUint32(" 1234", 0));

	CHECK_EQUAL(0u,		roStrToUint32("-1", 0));
	CHECK_EQUAL(maxU32,	roStrToUint32(" 4294967295", 0));
	CHECK_EQUAL(0u,		roStrToUint32(" 4294967296", 0));
}

TEST_FIXTURE(StringUtilityTest, integer64)
{
	const roInt64 max64 = ro::TypeOf<roInt64>::valueMax();
	const roInt64 min64 = ro::TypeOf<roInt64>::valueMin();
	const roUint64 maxU64 = ro::TypeOf<roUint64>::valueMax();

	// Not a number
	CHECK_EQUAL(123,	roStrToInt64("abc", 123));

	// Test for signed
	CHECK_EQUAL(0,		roStrToInt64("  0", 123));
	CHECK_EQUAL(-123,	roStrToInt64(" -123", 0));
	CHECK_EQUAL(-123,	roStrToInt64("- 123", 0));

	CHECK_EQUAL(min64,	roStrToInt64(" -9223372036854775808", 0));
	CHECK_EQUAL(0,		roStrToInt64(" -9223372036854775809", 0));
	CHECK_EQUAL(max64,	roStrToInt64("  9223372036854775807", 0));
	CHECK_EQUAL(0,		roStrToInt64("  9223372036854775809", 0));

	// Test for unsigned
	CHECK_EQUAL(0u,		roStrToUint64(" 0", 123));
	CHECK_EQUAL(1234u,	roStrToUint64(" 1234", 0));

	CHECK_EQUAL(0u,		roStrToUint64("-1", 0));
	CHECK_EQUAL(maxU64,	roStrToUint64(" 18446744073709551615", 0));
	CHECK_EQUAL(0u,		roStrToUint64(" 18446744073709551616", 0));
}

TEST_FIXTURE(StringUtilityTest, parseHex)
{
	CHECK_EQUAL(0,		roHexStrToUint64("0",	1, 123));
	CHECK_EQUAL(0,		roHexStrToUint64("0  ",	3, 123));
	CHECK_EQUAL(0,		roHexStrToUint64("  0",	3, 123));
	CHECK_EQUAL(1,		roHexStrToUint64("1",	1, 123));
	CHECK_EQUAL(2,		roHexStrToUint64("2",	1, 123));
	CHECK_EQUAL(10,		roHexStrToUint64("a",	1, 123));
	CHECK_EQUAL(15,		roHexStrToUint64("f",	1, 123));
	CHECK_EQUAL(10,		roHexStrToUint64("A",	1, 123));
	CHECK_EQUAL(15,		roHexStrToUint64("F",	1, 123));

	CHECK_EQUAL(123,	roHexStrToUint64("g",	1, 123));
	CHECK_EQUAL(123,	roHexStrToUint64("G",	1, 123));
	CHECK_EQUAL(123,	roHexStrToUint64("x",	1, 123));
	CHECK_EQUAL(123,	roHexStrToUint64("X",	1, 123));
	CHECK_EQUAL(123,	roHexStrToUint64("z",	1, 123));
	CHECK_EQUAL(123,	roHexStrToUint64("Z",	1, 123));

	CHECK_EQUAL(255,	roHexStrToUint64("fF",	2, 123));
}

TEST_FIXTURE(StringUtilityTest, toString)
{
	// Get the required size of the buffer
	CHECK_EQUAL(3u, roToString(NULL, 0, 123));
	CHECK_EQUAL(4u, roToString(NULL, 0, -123));
	CHECK_EQUAL(4u, roToString(NULL, 0, 1.23f));
	CHECK_EQUAL(4u, roToString(NULL, 0, 1.23));

	// Buffer of various size
	char buf[128] = { 0 };
	CHECK_EQUAL(0u, roToString(buf, 0, 123));
	CHECK_EQUAL(0u, roToString(buf, 1, 123));
	CHECK_EQUAL(0u, roToString(buf, 2, 123));
	CHECK_EQUAL(3u, roToString(buf, 3, 123));	CHECK_EQUAL("123", buf);
	CHECK_EQUAL(3u, roToString(buf, 4, 123));	CHECK_EQUAL("123", buf);
	CHECK_EQUAL(3u, roToString(buf, 5, 123));	CHECK_EQUAL("123", buf);

	CHECK_EQUAL(0u, roToString(buf, 0, -123));
	CHECK_EQUAL(0u, roToString(buf, 1, -123));
	CHECK_EQUAL(0u, roToString(buf, 2, -123));
	CHECK_EQUAL(0u, roToString(buf, 3, -123));
	CHECK_EQUAL(4u, roToString(buf, 4, -123));	CHECK_EQUAL("-123", buf);
	CHECK_EQUAL(4u, roToString(buf, 5, -123));	CHECK_EQUAL("-123", buf);
	CHECK_EQUAL(4u, roToString(buf, 6, -123));	CHECK_EQUAL("-123", buf);
	CHECK_EQUAL(0, roStrCmp(buf, "-123"));

	CHECK_EQUAL(0u, roToString(buf, 1, 1.23f));
	CHECK_EQUAL(0u, roToString(buf, 2, 1.23f));
	CHECK_EQUAL(4u, roToString(buf, 4, 1.23f));	CHECK_EQUAL("1.23", buf);
	CHECK_EQUAL(4u, roToString(buf, 8, 1.23f));	CHECK_EQUAL("1.23", buf);

	CHECK(roToString(buf, roCountof(buf), DBL_MAX) > 0);

	CHECK_EQUAL(7u, roToString(buf, roCountof(buf), (void*)0x123ABCD));	CHECK_EQUAL("123ABCD", buf);
}
