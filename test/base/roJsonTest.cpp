#include "pch.h"
#include "../../roar/base/roJson.h"
#include "../../roar/base/roIOStream.h"

using namespace ro;

class JsonTest {};

TEST_FIXTURE(JsonTest, writer_empty1)
{
    ro::JsonWriter writer;
    ro::MemoryOStream os;
}

TEST_FIXTURE(JsonTest, writer_empty2)
{
    ro::JsonWriter writer;
    ro::MemoryOStream os;

    writer.setStream(&os);
    writer.beginObject();
    writer.endObject();
}

TEST_FIXTURE(JsonTest, writer_numeric)
{
    ro::JsonWriter writer;
    ro::MemoryOStream os;

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
	ro::JsonWriter writer;
	ro::MemoryOStream os;

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
