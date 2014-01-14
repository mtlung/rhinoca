#include "pch.h"
#include "../../roar/math/roRandom.h"
#include "../../roar/base/roRingBuffer.h"

using namespace ro;

struct RingBufferTest {};

TEST_FIXTURE(RingBufferTest, empty)
{
	{
		RingBuffer ringBuffer;
	}

	{	RingBuffer ringBuffer;
		roByte* writePtr = NULL;
		CHECK(ringBuffer.write(0, writePtr));
		ringBuffer.commitWrite(0);

		roSize readSize = 0;
		CHECK(!ringBuffer.read(readSize));
		CHECK_EQUAL(0u, readSize);
	}
}

TEST_FIXTURE(RingBufferTest, atomicRead)
{
	// Write something
	RingBuffer ringBuffer;
	roByte* writePtr = NULL;
	CHECK(ringBuffer.write(2, writePtr));
	writePtr[0] = 0;
	writePtr[1] = 1;
	ringBuffer.commitWrite(2);

	// Read some but not all
	roSize readSize = 0;
	roByte* readPtr = ringBuffer.read(readSize);
	CHECK_EQUAL(2u, readSize);
	ringBuffer.commitRead(1u);

	// Then write some more
	CHECK(ringBuffer.write(2, writePtr));
	writePtr[0] = 2;
	writePtr[1] = 3;
	ringBuffer.commitWrite(2);

	// Then try to read 2 bytes atomically
	readSize = 2;
	CHECK(ringBuffer.atomicRead(readSize, readPtr));
	CHECK_EQUAL(1u, readPtr[0]);
	CHECK_EQUAL(2u, readPtr[1]);
	ringBuffer.commitRead(readSize);

	readSize = 2;
	CHECK(ringBuffer.atomicRead(readSize, readPtr) == roStatus::not_enough_data);

	ringBuffer.flushWrite(1);
}

// Write a deterministic sequence of bytes, and read them back to see if
// the sequence is still the same
TEST_FIXTURE(RingBufferTest, random)
{
	RingBuffer ringBuffer;
	ringBuffer.softLimit = 512;
	static const roSize chunkSize = 64;
	Random<UniformRandom> rand;					// For control purpose
	Random<UniformRandom> dataGen1, dataGen2;	// For verification purpose

	for(roSize iter1=0; iter1<10000; ++iter1)
	{
		roSize writeIterCount = rand.minMax(0u, 10u);
		for(roSize iter2=0; iter2<writeIterCount; ++iter2)
		{
			// Write some random amount of data to ring buffer
			roByte* writePtr;
			CHECK(ringBuffer.write(chunkSize, writePtr));

			roSize writeCount = rand.minMax<roSize>(0u, chunkSize);
			for(roSize i=0; i<writeCount; ++i)
				writePtr[i] = dataGen1.minMax<roByte>(0u, 128u);
			ringBuffer.commitWrite(writeCount);
		}

		// Read the buffer and verify it match with the deterministic sequence
		roSize readIterCount = rand.minMax(0u, 21u);
		for(roSize iter2=0; iter2<readIterCount; ++iter2)
		{
			roSize bytesRead = 0;
			roByte* readPtr = ringBuffer.read(bytesRead);
			bytesRead = rand.minMax<roSize>(0u, bytesRead);
			for(roSize i=0; i<bytesRead; ++i) {
				if(readPtr[i] != dataGen2.minMax(0u, 128u))
					goto Fail;
			}
			ringBuffer.commitRead(bytesRead);
		}
	}

	return;

Fail:
	CHECK(false);
}
