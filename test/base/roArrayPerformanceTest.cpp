#include "pch.h"
#include "../../roar/base/roArray.h"
#include "../../roar/base/roLog.h"
#include "../../roar/base/roStopWatch.h"
#include <vector>

using namespace ro;

class ArrayPerformanceTest {};

#define BEGIN_PROFILE(repeatCount, arySize, initCode) { \
	roLog("info", "%d x %d, ", repeatCount, arySize); \
	timer.reset(); \
	for(roSize i=0; i<sampleCount; ++i) for(roSize j=0; j<repeatCount; ++j) { initCode; for(roSize k=0; k<arySize; ++k) {

#define END_PROFILE() \
	}} roLog("info", "%f\n", timer.getFloat()); }

void roSwap(std::string& lhs, std::string& rhs)
{
	lhs.swap(rhs);
}

TEST_FIXTURE(ArrayPerformanceTest, compareWithSTL)
{
	ro::StopWatch timer;

	{	// Push back of int
		roLog("info", "Push back int\n");
		roSize sampleCount = roIsDebug ? 10 : 1000;

		typedef Array<roSize> MyArray;
		BEGIN_PROFILE(10000, 1, MyArray ary;)
			ary.pushBack(i);
		END_PROFILE();
		BEGIN_PROFILE(1000, 10, MyArray ary;)
			ary.pushBack(i);
		END_PROFILE();
		BEGIN_PROFILE(100, 100, MyArray ary;)
			ary.pushBack(i);
		END_PROFILE();
		BEGIN_PROFILE(10, 1000, MyArray ary;)
			ary.pushBack(i);
		END_PROFILE();
		BEGIN_PROFILE(1, 10000, MyArray ary;)
			ary.pushBack(i);
		END_PROFILE();

		typedef std::vector<roSize> STLVec;
		BEGIN_PROFILE(10000, 1, STLVec vec;)
			vec.push_back(i);
		END_PROFILE();
		BEGIN_PROFILE(1000, 10, STLVec vec;)
			vec.push_back(i);
		END_PROFILE();
		BEGIN_PROFILE(100, 100, STLVec vec;)
			vec.push_back(i);
		END_PROFILE();
		BEGIN_PROFILE(10, 1000, STLVec vec;)
			vec.push_back(i);
		END_PROFILE();
		BEGIN_PROFILE(1, 10000, STLVec vec;)
			vec.push_back(i);
		END_PROFILE();
	}

	{	// Push back of string
		roLog("\ninfo", "Push back std::string\n");
		roSize sampleCount = roIsDebug ? 1 : 100;

		typedef Array<std::string> MyArray;
		const std::string str("abcdefghijklmnopqrstuvwxyz");
		BEGIN_PROFILE(10000, 1, MyArray ary;)
			ary.pushBack(str);
		END_PROFILE();
		BEGIN_PROFILE(1000, 10, MyArray ary;)
			ary.pushBack(str);
		END_PROFILE();
		BEGIN_PROFILE(100, 100, MyArray ary;)
			ary.pushBack(str);
		END_PROFILE();
		BEGIN_PROFILE(10, 1000, MyArray ary;)
			ary.pushBack(str);
		END_PROFILE();
		BEGIN_PROFILE(1, 10000, MyArray ary;)
			ary.pushBack(str);
		END_PROFILE();

		typedef std::vector<std::string> STLVec;
		BEGIN_PROFILE(10000, 1, STLVec vec;)
			vec.push_back(str);
		END_PROFILE();
		BEGIN_PROFILE(1000, 10, STLVec vec;)
			vec.push_back(str);
		END_PROFILE();
		BEGIN_PROFILE(100, 100, STLVec vec;)
			vec.push_back(str);
		END_PROFILE();
		BEGIN_PROFILE(10, 1000, STLVec vec;)
			vec.push_back(str);
		END_PROFILE();
		BEGIN_PROFILE(1, 10000, STLVec vec;)
			vec.push_back(str);
		END_PROFILE();
	}
}
