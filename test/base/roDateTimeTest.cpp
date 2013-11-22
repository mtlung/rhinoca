#include "pch.h"
#include "../../roar/base/roStringFormat.h"
#include "../../roar/base/roDateTime.h"

using namespace ro;

TEST(DateTimeTest)
{
	DateTime t = DateTime::current();
	CHECK(t.year() >= 2013);
	CHECK(t.month() >= 1 && t.month() <= 12);
	CHECK(t.day() >= 1 && t.day() <= 31);

	String str;
	CHECK(strFormat(str, "{}", t));
	const roUtf8* s = str.c_str();
}
