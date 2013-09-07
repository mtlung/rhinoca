#include "pch.h"
#include "../../roar/base/roIOStream.h"

using namespace ro;

class IOStreamTest {};

TEST_FIXTURE(IOStreamTest, memoryIStream)
{
	roByte data[] = { 1, 2, 3, 4 };
	MemoryIStream is(data, roCountof(data));

	roUint64 size = 0;
	CHECK(is.size(size));
	CHECK_EQUAL(roCountof(data), size);

	// Test for read
	CHECK(!is.readWillBlock(1));

	roUint64 bytesRead = 0;
	CHECK(is.read(NULL, 0, bytesRead));

	CHECK(!is.read(NULL, 10, bytesRead));

	roByte tmpBuf[10] = {0};
	CHECK(is.read(tmpBuf, roCountof(tmpBuf), bytesRead));
	CHECK_EQUAL(roCountof(data), is.posRead());
	CHECK_EQUAL(roCountof(data), bytesRead);
	CHECK_EQUAL(1, tmpBuf[0]);
	CHECK_EQUAL(2, tmpBuf[1]);
	CHECK_EQUAL(3, tmpBuf[2]);
	CHECK_EQUAL(4, tmpBuf[3]);
	CHECK_EQUAL(0, tmpBuf[4]);

	CHECK(!is.read(tmpBuf, roCountof(tmpBuf), bytesRead));

	// Test for seek
	CHECK(is.seekRead(0, IStream::SeekOrigin_Begin));
	CHECK_EQUAL(0u, is.posRead());

	CHECK(!is.seekRead(-1, IStream::SeekOrigin_Current));
	CHECK_EQUAL(0u, is.posRead());

	CHECK(is.seekRead(0, IStream::SeekOrigin_End));
	CHECK_EQUAL(roCountof(data), is.posRead());

	CHECK(is.seekRead(0, IStream::SeekOrigin_Current));
	CHECK_EQUAL(roCountof(data), is.posRead());

	CHECK(is.seekRead(-1, IStream::SeekOrigin_Current));
	CHECK_EQUAL(roCountof(data) - 1, is.posRead());

	CHECK(!is.seekRead(2, IStream::SeekOrigin_Current));
	CHECK_EQUAL(roCountof(data) - 1, is.posRead());
}

TEST_FIXTURE(IOStreamTest, memoryOStream)
{
	roByte data[] = { 1, 2, 3, 4 };
	MemoryOStream os;

	CHECK_EQUAL(0, os.size());
	CHECK_EQUAL((roByte*)NULL, os.bytePtr());

	CHECK(os.write(data, 1));
	CHECK_EQUAL(1u, os.posWrite());

	CHECK(os.write(data + 1, 2));
	CHECK_EQUAL(3u, os.posWrite());

	CHECK_EQUAL(data[0], *os.bytePtr());

	// Test for seek
	CHECK(!os.seekWrite(0, OStream::SeekOrigin_Begin));
}

TEST_FIXTURE(IOStreamTest, memorySeekableOStream)
{
	roByte data[] = { 1, 2, 3, 4 };
	MemorySeekableOStream os;

	CHECK_EQUAL(0, os.size());
	CHECK_EQUAL((roByte*)NULL, os.bytePtr());

	CHECK(os.write(data, 1));
	CHECK_EQUAL(1u, os.posWrite());

	CHECK(os.write(data + 1, 2));
	CHECK_EQUAL(3u, os.posWrite());

	CHECK_EQUAL(data[0], *os.bytePtr());

	// Test for seek
	CHECK(!os.seekWrite(-1, OStream::SeekOrigin_Begin));
	CHECK(!os.seekWrite(os.size() + 1, OStream::SeekOrigin_Begin));
	CHECK(os.seekWrite(os.size(), OStream::SeekOrigin_Begin));
	CHECK(os.seekWrite(0, OStream::SeekOrigin_Begin));
	CHECK_EQUAL(0u, os.posWrite());

	CHECK(os.seekWrite(0, OStream::SeekOrigin_Begin));
	CHECK(!os.seekWrite(-1, OStream::SeekOrigin_Current));
	CHECK(!os.seekWrite(os.size() + 1, OStream::SeekOrigin_Current));
	CHECK(os.seekWrite(os.size(), OStream::SeekOrigin_Current));
	CHECK_EQUAL(os.size(), os.posWrite());

	CHECK(!os.seekWrite(-1, OStream::SeekOrigin_End));
	CHECK(os.seekWrite(0, OStream::SeekOrigin_End));
	CHECK_EQUAL(os.size(), os.posWrite());
	CHECK(os.seekWrite(os.size(), OStream::SeekOrigin_End));
	CHECK_EQUAL(0u, os.posWrite());
}
