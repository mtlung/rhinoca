#include "pch.h"
#include "../src/bipbuffer.h"
#include "../src/rhstring.h"

class BipBufferTest {};

TEST_FIXTURE(BipBufferTest, init)
{
	{	// Init with heap memory
		BipBuffer buf;
		CHECK(buf.init(NULL, 64, realloc));
	}

	{	// Init with stack memory
		BipBuffer buf;
		char rawBuf[64];
		CHECK(buf.init(rawBuf, sizeof(rawBuf), NULL));
	}

	{	// Init with stack and then heap memory
		BipBuffer buf;
		char rawBuf[64];
		CHECK(buf.init(rawBuf, sizeof(rawBuf), NULL));
		CHECK(buf.init(NULL, 64, realloc));
	}

	{	// Init with heap and then stack memory
		BipBuffer buf;
		CHECK(buf.init(NULL, 64, realloc));
		char rawBuf[64];
		CHECK(buf.init(rawBuf, sizeof(rawBuf), NULL));
	}
}

TEST_FIXTURE(BipBufferTest, basic)
{
	char rawBuf[2];

	// Init
	BipBuffer buf;
	CHECK(buf.init(rawBuf, sizeof(rawBuf), NULL));

	// In this test we want to transfer data from src to dest
	const char src[] = "hello world!";
	const char* srcBegin = src;
	const char* srcEnd = src + sizeof(src);
	String dest;

	do
	{
		while(true) {
			// Reserve space first
			unsigned reserved = 0;
			const unsigned chunkSize = rand() % sizeof(src);	// We use a random chunk size for maximum code coverage
			unsigned char* writePtr = buf.reserveWrite(chunkSize, reserved);

			if(!writePtr)
				break;

			// Do the write operation
			memcpy(writePtr, srcBegin, reserved);

			// Commit the written data
			// In this case, we commit what we have reserved, but committing something
			// less than the reserved is allowed.
			buf.commitWrite(reserved);

			srcBegin += reserved;
		}

		while(true) {
			unsigned readSize = 0;
			const unsigned char* readPtr = buf.getReadPtr(readSize);
			if(!readPtr)
				break;

			dest.append((const char*)readPtr, readSize);

			buf.commitRead(readSize);
		}
	} while(strcmp(dest.c_str(), src) != 0);

	CHECK(true);
}
