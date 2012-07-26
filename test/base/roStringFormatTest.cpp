#include "pch.h"
#include "../../roar/base/roStringFormat.h"
#include "../../roar/base/roTypeOf.h"

using namespace ro;

struct StringFormatTest {};

TEST_FIXTURE(StringFormatTest, basic)
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

	str.clear(); strFormat(str, "{} {} {} {} {} {} {} {}",
		TypeOf<roInt8>::valueMax(),
		TypeOf<roInt16>::valueMax(),
		TypeOf<roInt32>::valueMax(),
		TypeOf<roInt64>::valueMax(),
		TypeOf<roInt8>::valueMax(),
		TypeOf<roInt16>::valueMax(),
		TypeOf<roInt32>::valueMax(),
		TypeOf<roInt64>::valueMax()
	);
	CHECK(str == "127 32767 2147483647 9223372036854775807 127 32767 2147483647 9223372036854775807");

	str.clear(); strFormat(str, "{} {}", "Hello world!", 123);
	CHECK(str == "Hello world! 123");

	str.clear(); strFormat(str, "{} {}", String("Hello world!"), 456);
	CHECK(str == "Hello world! 456");
}
