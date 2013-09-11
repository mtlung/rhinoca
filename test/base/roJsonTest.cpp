#include "pch.h"
#include "../../roar/base/roJson.h"
#include "../../roar/base/roIOStream.h"

using namespace ro;

class JsonTest {};

TEST_FIXTURE(JsonTest, parse_empty)
{
    JsonParser parser;
	parser.parse("{}");

	JsonParser::Event::Enum e;
	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::BeginObject, e);

	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::EndObject, e);

	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::End, e);
}

TEST_FIXTURE(JsonTest, parse_basicType)
{
#define TEST_INT(str, val, eventType, getFunc) {\
	JsonParser parser; \
	parser.parse("{\"name\":" ##str "}"); \
	parser.nextEvent(); \
	parser.nextEvent(); \
	JsonParser::Event::Enum e = parser.nextEvent(); \
	CHECK_EQUAL(JsonParser::Event::eventType, e); \
	CHECK_EQUAL(val, parser.getFunc()); }

	TEST_INT("true",		true,			Bool,		getBool);
	TEST_INT("0",			0,				UInteger64,	getUint64);
	TEST_INT("123",			123,			UInteger64,	getUint64);
	TEST_INT("2147483648",	2147483648,		UInteger64,	getUint64);	// 2^31 - 1
	TEST_INT("4294967295",	4294967295,		UInteger64,	getUint64);
	TEST_INT("-123",		-123,			Integer64,	getInt64);
	TEST_INT("-2147483648",	-2147483648LL,	Integer64,	getInt64);	// -2^31 (min of int)

	TEST_INT("4294967296",			4294967296ULL,			UInteger64,	getUint64);	// 2^32
	TEST_INT("18446744073709551615",18446744073709551615ULL,UInteger64,	getUint64);	// 2^64 - 1

	TEST_INT("-2147483649",			-2147483649LL,			Integer64,	getInt64);	// -2^31 - 1
	TEST_INT("-9223372036854775808",-9223372036854775808LL,	Integer64,	getInt64);	// -2^63
#undef TEST_INT

#define TEST_DOUBLE(str, val) {\
	JsonParser parser; \
	parser.parse("{\"name\":" ##str "}"); \
	parser.nextEvent(); \
	parser.nextEvent(); \
	JsonParser::Event::Enum e = parser.nextEvent(); \
	CHECK_EQUAL(JsonParser::Event::Double, e); \
	CHECK_CLOSE(val, parser.getDouble(), 1e-309); }

	TEST_DOUBLE("0.0", 0.0);
	TEST_DOUBLE("1.0", 1.0);
	TEST_DOUBLE("-1.0", -1.0);
	TEST_DOUBLE("1.5", 1.5);
	TEST_DOUBLE("-1.5", -1.5);
	TEST_DOUBLE("3.1416", 3.1416);
	TEST_DOUBLE("1E10", 1E10);
	TEST_DOUBLE("1e10", 1e10);
	TEST_DOUBLE("1E+10", 1E+10);
	TEST_DOUBLE("1E-10", 1E-10);
	TEST_DOUBLE("-1E10", -1E10);
	TEST_DOUBLE("-1e10", -1e10);
	TEST_DOUBLE("-1E+10", -1E+10);
	TEST_DOUBLE("-1E-10", -1E-10);
	TEST_DOUBLE("1.234E+10", 1.234E+10);
	TEST_DOUBLE("1.234E-10", 1.234E-10);
	TEST_DOUBLE("1.79769e+308", 1.79769e+308);
//	TEST_DOUBLE("2.22507e-308", 2.22507e-308);	// TODO: underflow
	TEST_DOUBLE("-1.79769e+308", -1.79769e+308);
//	TEST_DOUBLE("-2.22507e-308", -2.22507e-308);	// TODO: underflow
	TEST_DOUBLE("18446744073709551616", 18446744073709551616.0);	// 2^64 (max of uint64_t + 1, force to use double)
	TEST_DOUBLE("-9223372036854775809", -9223372036854775809.0);	// -2^63 - 1(min of int64_t + 1, force to use double)

#undef TEST_DOUBLE

#define TEST_STRING(str, val) {\
	JsonParser parser; \
	parser.parse("{\"name\":" ##str "}"); \
	parser.nextEvent(); \
	parser.nextEvent(); \
	JsonParser::Event::Enum e = parser.nextEvent(); \
	CHECK_EQUAL(JsonParser::Event::String, e); \
	CHECK_EQUAL(val, parser.getString()); }

	TEST_STRING("\"\"", "");
	TEST_STRING("\"Hello\"", "Hello");
	TEST_STRING("\"\\\"\\\\/\\b\\f\\n\\r\\t\"", "\"\\/\b\f\n\r\t");
	TEST_STRING("\"\\u0024\"", "\x24");
	TEST_STRING("\"\\u0024\"", "\x24");
	TEST_STRING("\"\\u20AC\"", "\xE2\x82\xAC");
	TEST_STRING("\"\\uD834\\uDD1E\"", "\xF0\x9D\x84\x9E");	// G clef sign U+1D11E

#undef TEST_STRING

	JsonParser parser;
	parser.parse(
		"{\"null\":null,\"bool\":true,\"string\":\"This is a string\""
		"}"
	);

	JsonParser::Event::Enum e;
	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::BeginObject, e);

	{	e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::Name, e);
		CHECK_EQUAL("null", parser.getName());

		e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::Null, e);
	}

	{	e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::Name, e);
		CHECK_EQUAL("bool", parser.getName());

		e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::Bool, e);
		CHECK_EQUAL(true, parser.getBool());
	}

	{	e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::Name, e);
		CHECK_EQUAL("string", parser.getName());

		e = parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::String, e);
		CHECK_EQUAL("This is a string", parser.getString());
	}

	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::EndObject, e);

	e = parser.nextEvent();
	CHECK_EQUAL(JsonParser::Event::End, e);
}

TEST_FIXTURE(JsonTest, writer_empty1)
{
    JsonWriter writer;
    MemoryOStream os;
}

TEST_FIXTURE(JsonTest, writer_empty2)
{
    JsonWriter writer;
    MemoryOStream os;

    writer.setStream(&os);
    writer.beginObject();
    writer.endObject();
}

TEST_FIXTURE(JsonTest, writer_numeric)
{
    JsonWriter writer;
    MemoryOStream os;

    writer.setStream(&os);
    writer.beginObject();
        writer.write("int8", (roInt8)1);
        writer.write("int16", (roInt16)2);
        writer.write("int32", (roInt32)3);
        writer.write("int64", (roInt64)4);
        writer.write("uint8", (roUint8)1);
        writer.write("uint16", (roUint16)2);
        writer.write("uint32", (roUint32)3);
        writer.write("uint64", (roUint64)4);
        writer.write("float", 1.23f);
        writer.write("double", 1.23);
    writer.endObject();

    CHECK_EQUAL((roUtf8*)os._buf.bytePtr(), "{\"int8\":1,\"int16\":2,\"int32\":3,\"int64\":4,\"uint8\":1,\"uint16\":2,\"uint32\":3,\"uint64\":4,\"float\":1.23,\"double\":1.23}");
}

TEST_FIXTURE(JsonTest, writer)
{
	JsonWriter writer;
	MemoryOStream os;

	writer.setStream(&os);
	writer.beginObject();
	writer.write("firstName\n", "John\t");
	writer.write("lastName", "Smith");
	writer.write("age", 25);
	writer.beginObject("address");
		writer.write("streetAddress", "21 2nd Street");
		writer.write("city", "New York");
		writer.write("state", "NY");
		writer.write("postalCode", 10021);
	writer.endObject();
	writer.beginArray("phoneNumbers");
		writer.beginObject();
			writer.write("type", "home");
			writer.write("number", "212 555-1234");
		writer.endObject();
		writer.beginObject();
			writer.write("type", "fax");
			writer.write("number", "646 555-4567");
		writer.endObject();
	writer.endArray();
	roByte buf[] = { 0, 1, 2, 3 };
	writer.write("binary", buf, 4);
	writer.beginArray();
		writer.write("a", true);
		writer.write("b", false);
	writer.endArray();
	writer.endObject();
}
