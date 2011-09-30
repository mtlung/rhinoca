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
	// In this test we want to transfer data from src to dest
	const char src[] =
		"The Bip-Buffer is like a circular buffer, but slightly different. "
		"Instead of keeping one head and tail pointer to the data in the buffer, "
		"it maintains two revolving regions, allowing for fast data access without "
		"having to worry about wrapping at the end of the buffer";

	const char* srcBegin = src;
	const char* srcEnd = src + sizeof(src);
	String dest;

	// Init
	BipBuffer buf;
	CHECK(buf.init(NULL, (rand() % sizeof(src)) / 2, realloc));

	do
	{
		while(rand() % 2 != 0) {
			// Reserve space first
			unsigned reserved = 0;
			const unsigned chunkSize = (rand() % sizeof(src)) / 4 + 1;	// We use a random chunk size for maximum code coverage
			unsigned char* writePtr = buf.reserveWrite(chunkSize, reserved);

			if(!writePtr)
				break;

			// Do the write operation
			if(srcEnd - srcBegin < reserved)
				reserved = srcEnd - srcBegin;

			memcpy(writePtr, srcBegin, reserved);

			// Commit the written data
			// In this case, we commit what we have reserved, but committing something
			// less than the reserved is allowed.
			buf.commitWrite(reserved);

			srcBegin += reserved;
		}

		while(rand() % 2 != 0) {
			unsigned readSize = 0;

			// Get a read pointer to the contiguous block of memory
			const unsigned char* readPtr = buf.getReadPtr(readSize);
			if(!readPtr)
				break;

			dest.append((const char*)readPtr, readSize);

			// Tell the buffer you finish the read
			buf.commitRead(readSize);
		}
	} while(strcmp(dest.c_str(), src) != 0);

	CHECK(true);
}
