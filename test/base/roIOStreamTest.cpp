#include "pch.h"
#include "../../roar/base/roIOStream.h"
#include "../../roar/base/roString.h"
#include "../../roar/math/roRandom.h"

using namespace ro;

struct IOStreamTest {};

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

struct RandomIOStreamTest
{
	Status loadReferenceContent()
	{
		// Load the stream at once as a reference content
		roUint64 size;
		Status st = s->size(size);
		if(!st) return st;

		st = expected.resize(static_cast<roSize>(size));
		if(!st) return st;

		st = s->atomicRead(expected.c_str(), size);
		return st;
	}

	Status randomRead()
	{
		typedef Status (RandomIOStreamTest::*TestFunc)();
		TestFunc func[] = {
			&RandomIOStreamTest::_readWillBlock,
			&RandomIOStreamTest::_peek,
			&RandomIOStreamTest::_atomicPeek,
			&RandomIOStreamTest::_read,
			&RandomIOStreamTest::_atomicRead,
			&RandomIOStreamTest::_assertPointers,
		};

		actual.clear();

		// Do random operations on the stream to load the content
		Status st = Status::ok;
		for(roSize i=0; st; ++i)
		{
			roSize funcIdx = rand.beginEnd(0u, roCountof(func));
			st = (this->*func[funcIdx])();
		}

		if(st == Status::file_ended)
			st = Status::ok;

		return st;
	}

	bool verify()
	{
		return actual == expected;
	}

	Status _readWillBlock()
	{
		s->readWillBlock(rand.minMax(0u, 10u));
		return Status::ok;
	}

	Status _peek()
	{
		roByte* buf = NULL;
		roSize bytesReady = rand.minMax(0u, 10u);
		Status st = s->peek(buf, bytesReady);
		if(!st) return st;

		// If there is no error, we should always get something
		if(!buf || bytesReady == 0)
			return Status::unit_test_fail;

		if(roSize(s->end - s->current) < bytesReady)
			return Status::unit_test_fail;

		return st;
	}

	Status _atomicPeek()
	{
		roByte* buf = NULL;
		roSize bytesToPeek = rand.minMax(0u, 10u);
		Status st = s->peek(buf, bytesToPeek);
		if(!st) return st;

		if(roSize(s->end - s->current) < bytesToPeek)
			return Status::unit_test_fail;

		return st;
	}

	Status _atomicRead()
	{
		char buf[16];
		roUint64 bytesToRead = rand.minMax(0u, sizeof(buf));

		Status st = s->atomicRead(buf, bytesToRead);

		// If the file is already ended, just ignore it
		if(st == Status::file_ended)
			return Status::ok;

		if(!st)
			return st;

		return actual.append(buf, static_cast<roSize>(bytesToRead));
	}

	Status _read()
	{
		char buf[16];
		roUint64 maxBytesToRead = rand.minMax(0u, sizeof(buf));
		roUint64 actualBytesRead;

		Status st = s->read(buf, maxBytesToRead, actualBytesRead);
		if(!st)
			return st;

		if(actualBytesRead > maxBytesToRead)
			return Status::unit_test_fail;

		st = actual.append(buf, static_cast<roSize>(actualBytesRead));
		if(!st) return st;

		return st;
	}

	Status _assertPointers()
	{
		if(s->st == Status::file_ended)
			return Status::ok;

		if(!(s->begin <= s->current && s->current <= s->end))
			return Status::unit_test_fail;

		return Status::ok;
	}

	String actual;
	String expected;
	AutoPtr<IStream> s;
	Random<UniformRandom> rand;
};

TEST_FIXTURE(RandomIOStreamTest, rawFile)
{
	CHECK(openRawFileIStream("Test.vc9.vcproj", s));
	CHECK(loadReferenceContent());
	for(roSize i=0; i<128; ++i) {
		CHECK(openRawFileIStream("Test.vc9.vcproj", s));
		CHECK(randomRead());
		CHECK(verify());
	}
}
