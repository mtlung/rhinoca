#ifndef __math_roRandom_h__
#define __math_roRandom_h__

#include <stdlib.h>
#include "../base/roMutex.h"

namespace ro {

// Interface to do query on the underneath generator
// The RandGen class must provide:
// 1. A constructor taking a seed as parameter
// 2. A nextSeed() function that will update and return the seed (include zero)
// 3. A maxValueInv() function
template<class RandGen, class MutexType=NullMutex>
struct Random : public RandGen
{
	Random() {}
	Random(roUint32 seed) : RandGen(seed) {}

	// Range = [0, 1]
	float		randf();

	// Range = [min, max]
	float		randf(float min, float max);

	roUint32	randUint32();

	// Range = [begin, end)
	// (inclusive begin, exclusive end)
	template<class T>
	T			beginEnd(T begin, T end);

	// Range = [min, max]
	// (inclusive min, inclusive max)
	template<class T>
	T			minMax(T begin, T end);

	MutexType	mutex;
};	// Random

// Range [0, 16777215]
// Reference: Numerical Recipes Ch.7 p.284
// Reference: http://c-faq.com/lib/rand.html
struct UniformRandom
{
	UniformRandom() : seed(1) {}
	UniformRandom(roUint32 s) {
		seed = s;
	}

	roUint32 nextSeed()
	{
		seed = 1664525 * seed + 1013904223;
		return (seed >> 8);	// Remove the 8 LSB since they have the smallest period before repeating
	}

	float maxValueInv()
	{
		return 1.0f / 0x00FFFFFF;
	}

	roUint32 seed;
};	// UniformRandom


//////////////////////////////////////////////////////////////////////////
// Implementation

template<class RandGen, class MutexType>
float Random<RandGen, MutexType>::randf()
{
	mutex.lock();
	float ret = (float)(RandGen::nextSeed() * RandGen::maxValueInv());
	mutex.unlock();
	roAssert(ret >= 0.0f);
	roAssert(ret <= 1.0f);
	return ret;                 
}

template<class RandGen, class MutexType>
float Random<RandGen, MutexType>::randf(float min, float max)
{
	// Get Number between 0 and 1.0
	float fFact = randf();
	roAssert(fFact >= 0 && fFact <= 1.0f);

	// Map the Result to the required interval
	float ret = min + ((max - min) * fFact);

	return ret;
}

template<class RandGen, class MutexType>
roUint32 Random<RandGen, MutexType>::randUint32()
{
	mutex.lock();
	roUint32 ret = RandGen::nextSeed();
	mutex.unlock();
	return ret;
}

template<class RandGen, class MutexType>
template<class T>
T Random<RandGen, MutexType>::beginEnd(T begin, T end)
{
	// Undefined when begin == end, because of inclusive/exclusive endpoints
	roAssert(begin < end);

	// Prevent a division by 0
	if(begin == end)
		return begin;

	roUint32 res = randUint32();
	return begin + T(res % roUint32(end - begin));
}

template<class RandGen, class MutexType>
template<class T>
T Random<RandGen, MutexType>::minMax(T min, T max)
{
	roAssert(min <= max);

	// Prevent a division by 0
	if(max - min + 1 == 0)
		return min;

	roUint32 res = randUint32();
	return min + T(res % roUint32(max - min + 1));
}

}	// namespace ro

#endif	// __math_roRandom_h__
