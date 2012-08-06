#include "pch.h"
#include "../../roar/base/roStringFormat.h"
#include "../../roar/base/roTypeOf.h"

using namespace ro;

struct StringFormatTest {};

TEST_FIXTURE(StringFormatTest, padding)
{
	String str;
	str = "123";
	strPaddingLeft(str, ' ', 0);
	CHECK(str == "123");

	str = "123";
	strPaddingLeft(str, ' ', 3);
	CHECK(str == "123");

	str = "123";
	strPaddingLeft(str, ' ', 10);
	CHECK(str == "       123");

	str = "123";
	strPaddingRight(str, ' ', 0);
	CHECK(str == "123");

	str = "123";
	strPaddingRight(str, ' ', 3);
	CHECK(str == "123");

	str.insert(0, '0');

	str.insert(1, "abc", 3);

	str = "123";
	strPaddingRight(str, ' ', 10);
	CHECK(str == "123       ");
}

TEST_FIXTURE(StringFormatTest, printf)
{
	String str;

	str.clear(); strFormat(str, "{}", TypeOf<roInt8>::valueMax());
	CHECK(str == "127");

	str.clear(); strFormat(str, "{}", TypeOf<roInt16>::valueMax());
	CHECK(str == "32767");

	str.clear(); strFormat(str, "{}", TypeOf<roInt32>::valueMax());
	CHECK(str == "2147483647");

	str.clear(); strFormat(str, "{}", TypeOf<roInt64>::valueMax());
	CHECK(str == "9223372036854775807");

	str.clear(); strFormat(str, "{} {} {} {} {} {} {} {} {} {}",
		TypeOf<roInt8>::valueMax(),
		TypeOf<roInt16>::valueMax(),
		TypeOf<roInt32>::valueMax(),
		TypeOf<roInt64>::valueMax(),
		TypeOf<roInt8>::valueMax(),
		TypeOf<roInt16>::valueMax(),
		TypeOf<roInt32>::valueMax(),
		TypeOf<roInt64>::valueMax(),
		1.23456f,
		1.23456
	);
	CHECK(str == "127 32767 2147483647 9223372036854775807 127 32767 2147483647 9223372036854775807 1.23 1.23");

	str.clear(); strFormat(str, "{} {}", "Hello world!", 123);
	CHECK(str == "Hello world! 123");

	str.clear(); strFormat(str, "{} {}", String("Hello world!"), 456);
	CHECK(str == "Hello world! 456");
}

TEST_FIXTURE(StringFormatTest, printf_padding)
{
	String str;

	// Padding left, total length 5, pad with ' '
	str.clear(); strFormat(str, "{pl5 }", TypeOf<roInt8>::valueMax());
	CHECK(str == "  127");

	// Padding right, total length 5, pad with '-'
	str.clear(); strFormat(str, "{pr5-}", TypeOf<roInt8>::valueMax());
	CHECK(str == "127--");
}
