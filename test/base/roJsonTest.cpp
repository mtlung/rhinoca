#include "pch.h"
#include "../../roar/base/roJson.h"
#include "../../roar/base/roIOStream.h"

using namespace ro;

class JsonTest {};

TEST_FIXTURE(JsonTest, parse_empty)
{
	JsonParser parser;

	JsonParser::Event::Enum e;
	CHECK_EQUAL(JsonParser::Event::Error, parser.nextEvent());

	parser.parseInplace("");
	CHECK_EQUAL(JsonParser::Event::Error, parser.nextEvent());

	parser.parseInplace(NULL);
	CHECK_EQUAL(JsonParser::Event::Error, parser.nextEvent());

	parser.parse("{}");

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
	parser.parse("[" ##str "]"); \
	parser.nextEvent(); \
	JsonParser::Event::Enum e = parser.nextEvent(); \
	CHECK_EQUAL(JsonParser::Event::eventType, e); \
	CHECK_EQUAL(val, parser.getFunc()); }

	TEST_INT("true",		true,			Bool,		getBool);
	TEST_INT("0",			0,				UInteger64,	getUint64);
	TEST_INT("123",			123,			UInteger64,	getUint64);
	TEST_INT("2147483648",	2147483647,		UInteger64,	getUint64);	// 2^31 - 1
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
	parser.parse("[" ##str "]"); \
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
//	TEST_DOUBLE("2.22507e-308", 2.22507e-308);	// TODO: report underflow instead of getting zero
	TEST_DOUBLE("-1.79769e+308", -1.79769e+308);
//	TEST_DOUBLE("-2.22507e-308", -2.22507e-308);	// TODO: report underflow instead of getting zero
	TEST_DOUBLE("18446744073709551616", 18446744073709551616.0);	// 2^64 (max of uint64_t + 1, force to use double)
	TEST_DOUBLE("-9223372036854775809", -9223372036854775809.0);	// -2^63 - 1(min of int64_t + 1, force to use double)

#undef TEST_DOUBLE

#define TEST_STRING(str, val) {\
	JsonParser parser; \
	parser.parse("[" ##str "]"); \
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

	CHECK_EQUAL(JsonParser::Event::BeginObject, parser.nextEvent());

	CHECK_EQUAL(JsonParser::Event::Name, parser.nextEvent());
	CHECK_EQUAL("null", parser.getName());
	CHECK_EQUAL(JsonParser::Event::Null, parser.nextEvent());

	CHECK_EQUAL(JsonParser::Event::Name, parser.nextEvent());
	CHECK_EQUAL("bool", parser.getName());
	CHECK_EQUAL(JsonParser::Event::Bool, parser.nextEvent());
	CHECK_EQUAL(true, parser.getBool());

	CHECK_EQUAL(JsonParser::Event::Name, parser.nextEvent());
	CHECK_EQUAL("string", parser.getName());
	CHECK_EQUAL(JsonParser::Event::String, parser.nextEvent());
	CHECK_EQUAL("This is a string", parser.getString());

	CHECK_EQUAL(JsonParser::Event::EndObject, parser.nextEvent());
	CHECK_EQUAL(JsonParser::Event::End, parser.nextEvent());
}

TEST_FIXTURE(JsonTest, get_generic_number)
{
	JsonParser parser;
	parser.parse("[-123, 256, 1.23, 3.5e38]");
	parser.nextEvent();

	{	CHECK(parser.isNumber(parser.nextEvent()));

		roInt8 i8;
		CHECK(parser.getNumber(i8));
		CHECK_EQUAL(-123, i8);

		roUint8 u8;
		CHECK(!parser.getNumber(u8));

		float f;
		CHECK(parser.getNumber(f));
		CHECK_EQUAL(-123, f);
	}

	{	CHECK(parser.isNumber(parser.nextEvent()));

		roUint8 u8;
		CHECK(!parser.getNumber(u8));

		roInt16 i16;
		CHECK(parser.getNumber(i16));
		CHECK_EQUAL(256, i16);
	}

	{	CHECK(parser.isNumber(parser.nextEvent()));

		roInt8 i8;
		CHECK(!parser.getNumber(i8));

		float f;
		CHECK(parser.getNumber(f));
		CHECK_EQUAL(1.23f, f);

		double d;
		CHECK(parser.getNumber(d));
		CHECK_EQUAL(1.23, d);
	}

	{	CHECK(parser.isNumber(parser.nextEvent()));

		float f;
		CHECK(!parser.getNumber(f));

		double d;
		CHECK(parser.getNumber(d));
		CHECK_EQUAL(3.5e38, d);
	}
}

TEST_FIXTURE(JsonTest, parse_array)
{
	{	// Object as root
		JsonParser parser;
		parser.parse("{\"empty\":[]}");
		parser.nextEvent();
		parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
	}

	{	// Array as root
		JsonParser parser;
		parser.parse("[]");
		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());

		parser.parse("[{}]");
		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::BeginObject, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::EndObject, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
	}

	{	JsonParser parser;
		parser.parse("{\"2 ele\":[1,2]}");
		parser.nextEvent();
		parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::UInteger64, parser.nextEvent());
		CHECK_EQUAL(1u, parser.getUint64());
		CHECK_EQUAL(JsonParser::Event::UInteger64, parser.nextEvent());
		CHECK_EQUAL(2u, parser.getUint64());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
		CHECK_EQUAL(2, parser.getArrayElementCount());
	}

	{	// Nested array
		JsonParser parser;
		parser.parse("{\"nested\":[[1],[11,12]]}");
		parser.nextEvent();
		parser.nextEvent();
		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());

		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::UInteger64, parser.nextEvent());
		CHECK_EQUAL(1u, parser.getUint64());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
		CHECK_EQUAL(1u, parser.getArrayElementCount());

		CHECK_EQUAL(JsonParser::Event::BeginArray, parser.nextEvent());
		CHECK_EQUAL(JsonParser::Event::UInteger64, parser.nextEvent());
		CHECK_EQUAL(11u, parser.getUint64());
		CHECK_EQUAL(JsonParser::Event::UInteger64, parser.nextEvent());
		CHECK_EQUAL(12u, parser.getUint64());
		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
		CHECK_EQUAL(2u, parser.getArrayElementCount());

		CHECK_EQUAL(JsonParser::Event::EndArray, parser.nextEvent());
		CHECK_EQUAL(2u, parser.getArrayElementCount());

		CHECK_EQUAL(JsonParser::Event::EndObject, parser.nextEvent());
	}
}

TEST_FIXTURE(JsonTest, parse_error)
{
	{	// Forgot to call parse()
		JsonParser parser;
		CHECK_EQUAL(JsonParser::Event::Error, parser.nextEvent());
	}

#define TEST_ERROR(str) {\
	JsonParser parser; \
	parser.parse(str); \
	while(true) { \
		JsonParser::Event::Enum e = parser.nextEvent(); \
		if(e == JsonParser::Event::Error) break; \
		CHECK(e != JsonParser::Event::End); \
		if(e == JsonParser::Event::End) break; \
	}}

	// Text only contains white space(s)
	TEST_ERROR("");
	TEST_ERROR(" ");
	TEST_ERROR(" \n");

	// Expect either an object or array at root
	TEST_ERROR("null");
	TEST_ERROR("true");
	TEST_ERROR("false");
	TEST_ERROR("\"s\"");
	TEST_ERROR("0");

	// Invalid value
	TEST_ERROR("nulL");
	TEST_ERROR("truE");
	TEST_ERROR("falsE");

	// Name of an object member must be a string
	TEST_ERROR("{null:1}");
	TEST_ERROR("{true:1}");
	TEST_ERROR("{false:1}");
	TEST_ERROR("{1:1}");
	TEST_ERROR("{[]:1}");
	TEST_ERROR("{{}:1}");
	TEST_ERROR("{xyz:1}");

	// There must be a colon after the name of object member
	TEST_ERROR("{\"a\" 1}");
	TEST_ERROR("{\"a\",1}");

	// Must be a comma or '}' after an object member
	TEST_ERROR("{]");
	TEST_ERROR("{\"a\":1]");

#undef TEST_ERROR
}

TEST_FIXTURE(JsonTest, writer_empty1)
{
	{	JsonWriter writer;
		MemoryOStream os;
	}

	{	JsonWriter writer;
		MemoryOStream os;
		writer.setStream(&os);
		writer.beginDocument();
		writer.endDocument();
	}
}

TEST_FIXTURE(JsonTest, writer_empty2)
{
	JsonWriter writer;
	MemoryOStream os;

	writer.setStream(&os);
	writer.beginDocument();
	CHECK(writer.beginObject());
	CHECK(writer.endObject());
	writer.endDocument();
}

TEST_FIXTURE(JsonTest, writer_numeric)
{
	JsonWriter writer;
	MemoryOStream os;

	writer.setStream(&os);
	writer.beginDocument();
	CHECK(writer.beginObject());
		CHECK(writer.write("int8", (roInt8)1));
		CHECK(writer.write("int16", (roInt16)2));
		CHECK(writer.write("int32", (roInt32)3));
		CHECK(writer.write("int64", (roInt64)4));
		CHECK(writer.write("uint8", (roUint8)1));
		CHECK(writer.write("uint16", (roUint16)2));
		CHECK(writer.write("uint32", (roUint32)3));
		CHECK(writer.write("uint64", (roUint64)4));
		CHECK(writer.write("float", 1.23f));
		CHECK(writer.write("double", 1.23));
	CHECK(writer.endObject());
	writer.endDocument();

	CHECK_EQUAL((roUtf8*)os._buf.bytePtr(), "{\"int8\":1,\"int16\":2,\"int32\":3,\"int64\":4,\"uint8\":1,\"uint16\":2,\"uint32\":3,\"uint64\":4,\"float\":1.23,\"double\":1.23}");
}

TEST_FIXTURE(JsonTest, writer)
{
	JsonWriter writer;
	MemoryOStream os;

	writer.setStream(&os);
	writer.beginDocument();
	CHECK(writer.beginObject());
	CHECK(writer.write("firstName\n", "John\t"));
	CHECK(writer.write("lastName", "Smith"));
	CHECK(writer.write("age", 25));
	CHECK(writer.beginObject("address"));
		CHECK(writer.write("streetAddress", "21 2nd Street"));
		CHECK(writer.write("city", "New York"));
		CHECK(writer.write("state", "NY"));
		CHECK(writer.write("postalCode", 10021));
	CHECK(writer.endObject());
	CHECK(writer.beginArray("phoneNumbers"));
		CHECK(writer.beginObject());
			CHECK(writer.write("type", "home"));
			CHECK(writer.write("number", "212 555-1234"));
		CHECK(writer.endObject());
		CHECK(writer.beginObject());
			CHECK(writer.write("type", "fax"));
			CHECK(writer.write("number", "646 555-4567"));
		CHECK(writer.endObject());
	CHECK(writer.endArray());
	roByte buf[] = { 0, 1, 2, 3 };
	CHECK(writer.write("binary", buf, 4));
	CHECK(writer.beginArray());
		CHECK(writer.write(true));
		CHECK(writer.write(false));
	CHECK(writer.endArray());
	CHECK(writer.endObject());
	writer.endDocument();
}

#include "../../roar/base/roBlockAllocator.h"

TEST_FIXTURE(JsonTest, dom_parsing)
{
	String str =
		"{\"menu\": {"
		"	\"id\": \"file\","
		"		\"value\": \"File\","
		"		\"popup\": {"
		"			\"menuitem\": ["
		"			{\"value\": \"New\", \"onclick\": \"CreateNewDoc()\"},"
		"			{\"value\": \"Open\", \"onclick\": \"OpenDoc()\"},"
		"			{\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}"
		"			]"
		"	}"
		"}}";

	BlockAllocator allocator(sizeof(JsonValue) * 64);
	JsonValue* json = jsonParseInplace(str.c_str(), allocator);

	CHECK_EQUAL(JsonParser::Event::BeginObject, json->type);
	CHECK_EQUAL(1, json->memberCount);
}

TEST_FIXTURE(JsonTest, dom_parsing_error)
{
	String str = "";	// Root must be object or array
	String errorStr;

	BlockAllocator allocator(sizeof(JsonValue) * 64);
	JsonValue* json = jsonParseInplace(str.c_str(), allocator, &errorStr);

	CHECK(!json);
}

#include "../../roar/base/roRawFileSystem.h"

TEST_FIXTURE(JsonTest, parse_bigFile)
{
	void* file = NULL;
	Status st = rawFileSystemOpenFile("test.json", file);

	while(st && rawFileSystemReadWillBlock(file, roUint64(-1))) {}

	roUint64 size;
	char* buf = rawFileSystemGetBuffer(file, roUint64(-1), size);
	CHECK(buf);

	JsonParser parser;
	parser.parseInplace(buf);

	roSize i=0;
	while(parser.nextEvent() > 0)
	{
		++i;
	}
}
