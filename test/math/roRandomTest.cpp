#include "pch.h"
#include "../../roar/math/roRandom.h"
#include "../../roar/base/roArray.h"

using namespace ro;

struct RandomTest
{
};

TEST_FIXTURE(RandomTest, uniform)
{
	{
		Random<UniformRandom> rand(0);
		StaticArray<roSize, 4> bucket = {0};

		for(roSize i=0; i<1000000; ++i)
		{
			roSize index = rand.beginEnd(0u, bucket.size());
			bucket[index]++;
		}

		CHECK(true);
	}

	{
		Random<UniformRandom> rand(0);
		StaticArray<roSize, 4> bucket = {0};

		for(roSize i=0; i<1000000; ++i)
		{
			float f = rand.randf();
			roSize index = roSize(f * bucket.size());
			if(index >= bucket.size())
				continue;

			bucket[index]++;
		}

		CHECK(true);
	}
}
