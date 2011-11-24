#include "pch.h"
#include "../src/map.h"
#include "../roar/base/roStopWatch.h"
#include <map>

using namespace ro;

namespace {

static const bool benchmark = false;
static const unsigned testCount = 99999;

class MapPerformanceTest {};

struct MyNode : public MapBase<int>::Node<MyNode>
{
	typedef MapBase<int>::Node<MyNode> Super;
	MyNode(int key, int val)
		: Super(key), val(val)
	{}
	int val;
};	// MyNode

}	// namespace

TEST_FIXTURE(MapPerformanceTest, podType)
{
	if(!benchmark)
		return;

	{	printf("Our map:\n");
		Map<MyNode> map;
		StopWatch stopWatch;
		float begin = stopWatch.getFloat();
		for(unsigned i=0; i<testCount; ++i) {
			map.insert(*new MyNode(i, i));
		}
		for(unsigned i=testCount; i--;) {
			map.insert(*new MyNode(i, i));
		}
		for(unsigned i=testCount; i--;) {
			int v = rand();
			map.insert(*new MyNode(v, v));
		}

		printf("Time for insert: %f\n", stopWatch.getFloat() - begin);

		stopWatch.reset();
		begin = stopWatch.getFloat();
		for(unsigned i=0; i<testCount; ++i) {
			CHECK(map.find(i));
		}
		bool found = false;
		for(unsigned i=testCount; i--;) {
			int v = rand();
			found |= map.find(v) != NULL;
		}
		CHECK(found);
		printf("Time for find: %f\n", stopWatch.getFloat() - begin);

		stopWatch.reset();
		map.destroyAll();
		printf("Time for destroyAll: %f\n", stopWatch.getFloat() - begin);
	}

	{	printf("std::map:\n");
		std::multimap<int, int> map;
		StopWatch stopWatch;
		float begin = stopWatch.getFloat();
		for(unsigned i=0; i<testCount; ++i) {
			map.insert(std::make_pair(i, i));
		}
		for(unsigned i=testCount; i--;) {
			map.insert(std::make_pair(i, i));
		}
		for(unsigned i=testCount; i--;) {
			int v = rand();
			map.insert(std::make_pair(v, v));
		}
		printf("Time for insert: %f\n", stopWatch.getFloat() - begin);

		stopWatch.reset();
		begin = stopWatch.getFloat();
		for(unsigned i=0; i<testCount; ++i) {
			CHECK(map.find(i) != map.end());
		}
		bool found = false;
		for(unsigned i=testCount; i--;) {
			int v = rand();
			found |= map.find(v) != map.end();
		}
		CHECK(found);
		printf("Time for find: %f\n", stopWatch.getFloat() - begin);

		stopWatch.reset();
		map.clear();
		printf("Time for clear: %f\n", stopWatch.getFloat() - begin);
	}
}
