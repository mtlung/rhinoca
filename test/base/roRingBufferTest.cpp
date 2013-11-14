#include "pch.h"
#include "../../roar/math/roRandom.h"
#include "../../roar/base/roRingBuffer.h"

using namespace ro;

// Write a deterministic sequence of bytes, and read them back to see if
// the sequence is still the same
TEST(RingBufferTest)
{
	RingBuffer ringBuffer;
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

			roSize writeCount = rand.minMax(0u, chunkSize);
			for(roSize i=0; i<writeCount; ++i)
				writePtr[i] = dataGen1.minMax(0u, 128u);
			ringBuffer.commitWrite(writeCount);
		}

		// Read the buffer and verify it match with the deterministic sequence
		roSize readIterCount = rand.minMax(0u, 11u);
		for(roSize iter2=0; iter2<readIterCount; ++iter2)
		{
			roSize bytesRead = rand.minMax(0u, 2048u);
			roByte* readPtr = ringBuffer.read(bytesRead);
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
