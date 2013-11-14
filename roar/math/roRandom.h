#ifndef __math_roRandom_h__
#define __math_roRandom_h__

#include <stdlib.h>
#include "../platform/roCompiler.h"

namespace ro {

// Interface to do query on the underneath generator
// The RandGen class must provide:
// 1. A constructor taking a seed as parameter
// 2. A nextSeed() function that will update and return the seed
// 3. A maxValueInv() function
template<class RandGen>
struct Random : public RandGen
{
	Random() {}
	Random(roUint32 seed) : RandGen(seed) {}

	// Range = (0, 1)
	float		randf();

	// Range = (min, max)
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
};	// Random

// Range [1, 2147483646]
// Reference: http://c-faq.com/lib/rand.html
struct UniformRandom
{
	UniformRandom() : seed(1) {}
	UniformRandom(roUint32 s) {
		if(s == 0) seed = 1;
		if(s == m) seed = m - 1;
	}

	roUint32 nextSeed()
	{
		static const roInt32 q = m / a;
		static const roInt32 r = m % a;

		roInt32 hi = seed / q;
		roInt32 lo = seed % q;
		roInt32 test = a * lo - r * hi;
		if(test > 0)
			seed = test;
		else
			seed = test + m;

		return seed;
	}

	float maxValueInv()
	{
		return 1.0f / m;
	}

	roInt32 seed;
	static const roInt32 a = 16807u;
	static const roInt32 m = 2147483647u;
};	// UniformRandom


//////////////////////////////////////////////////////////////////////////
// Implementation

template<class RandGen>
float Random<RandGen>::randf()
{
	float ret = (float)(RandGen::nextSeed() * RandGen::maxValueInv());
	roAssert(ret >= 0.0f);
	roAssert(ret <= 1.0f);
	return ret;                 
}

template<class RandGen>
float Random<RandGen>::randf(float min, float max)
{
	// Get Number between 0 and 1.0
	float fFact = randf();

	// Map the Result to the required interval
	float ret = min + ((max - min) * fFact);

	return ret;
}

template<class RandGen>
roUint32 Random<RandGen>::randUint32()
{
	return RandGen::nextSeed();
}

template<class RandGen>
template<class T>
T Random<RandGen>::beginEnd(T begin, T end)
{
	// Undefined when begin == end, because of inclusive/exclusive endpoints
	roAssert(begin < end);

	// Prevent a division by 0
	if(begin == end)
		return begin;

	roUint32 res = randUint32();
	return begin + T(res % roUint32(end - begin));
}

template<class RandGen>
template<class T>
T Random<RandGen>::minMax(T min, T max)
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
