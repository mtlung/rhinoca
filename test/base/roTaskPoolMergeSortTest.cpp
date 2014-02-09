#include "pch.h"
#include "../../roar/base/roTaskPool.h"
#include "../../roar/base/roStopWatch.h"
#include "../../roar/base/roArray.h"
#include <stdlib.h>

using namespace ro;

static const bool benchmark = false;
static const rhuint testSize = benchmark ? 10000000 : 1000;

/// scratch is a memory buffer having the same size as data
static void merge(int* data, int* scratch, roSize left, roSize leftEnd, roSize right, roSize rightEnd)
{
	// The merge operation is much faster with a scratch memory buffer
	if(scratch) {
		roSize begin = left;
		roSize end = rightEnd;
		for(roSize i=begin; i<end; ++i) {
			if(right >= rightEnd || (left < leftEnd && data[left] <= data[right]))
				scratch[i] = data[left++];
			else
				scratch[i] = data[right++];
		}
		for(roSize i=begin; i<end; ++i)
			data[i] = scratch[i];
	}
	// Otherwise we fallback to a memmove solution
	else {
		for(;left < rightEnd && right < rightEnd; ++left) {
			int val = data[right];
			if(left >= right || val >= data[left]) continue;
			memmove(data + left + 1, data + left, sizeof(int) * (right - left));
			data[left] = val;
			++right;
		}
	}
}

static void mergeSortSingleThread(int* data, int* scratch, roSize begin, roSize end)
{
	roSize middle = (end - begin) / 2;
	if(middle == 0) return;

	roSize left = begin;
	roSize leftEnd = begin + middle;
	roSize right = leftEnd;
	roSize rightEnd = end;

	mergeSortSingleThread(data, scratch, left, leftEnd);

	mergeSortSingleThread(data, scratch, right, rightEnd);

	merge(data, scratch, left, leftEnd, right, rightEnd);
}

TEST(TaskPoolSingleThreadMergeSortTest)
{
	Array<int> data(testSize), scratch(testSize);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	StopWatch stopWatch;
	float begin = stopWatch.getFloat();
	mergeSortSingleThread(&data[0], &scratch[0], 0, data.size());
	float dt =  stopWatch.getFloat() - begin;

	if(benchmark)
		printf("Time for SingleThreadMergeSort: %f\n", dt);

	for(rhuint i=1; i<data.size(); ++i)
		CHECK(data[i-1] <= data[i]);
}

static void mergeSortMultiThread(int* data, int* scratch, roSize begin, roSize end, TaskPool& taskPool, roSize workCount=1)
{
	roSize middle = (end - begin) / 2;
	if(middle == 0) return;

	roSize left = begin;
	roSize leftEnd = begin + middle;
	roSize right = leftEnd;
	roSize rightEnd = end;

	class MergeSortTask : public Task
	{
	public:
		MergeSortTask(int* data, int* scratch, roSize begin, roSize end, roSize workCount)
			: _data(data), _scratch(scratch), _begin(begin), _end(end), _workCount(workCount)
		{}

		void run(TaskPool* taskPool) override
		{
			mergeSortMultiThread(_data, _scratch, _begin, _end, *taskPool, _workCount * 2);
			delete this;
		}

		int *_data, *_scratch;
		roSize _begin, _end, _workCount;
	};

	class MergeTask : public Task
	{
	public:
		MergeTask(int* data, int* scratch, roSize left, roSize leftEnd, roSize right, roSize rightEnd)
			: _data(data), _scratch(scratch), _left(left), _leftEnd(leftEnd), _right(right), _rightEnd(rightEnd)
		{}

		void run(TaskPool* taskPool) override
		{
			merge(_data, _scratch, _left, _leftEnd, _right, _rightEnd);
			delete this;
		}

		int *_data, *_scratch;
		roSize _left, _leftEnd, _right, _rightEnd;
	};

	class DepdencyTask : public Task
	{
	public:
		DepdencyTask(TaskId t1, TaskId t2)
			: _t1(t1), _t2(t2)
		{}

		void run(TaskPool* taskPool) override
		{
			taskPool->wait(_t1);
			taskPool->wait(_t2);
			delete this;
		}

		TaskId _t1, _t2;
	};

	if(workCount == 4)
		mergeSortSingleThread(data, scratch, begin, end);
	else
	{
		TaskId task1 = taskPool.addFinalized(new MergeSortTask(data, scratch, left, leftEnd, workCount));
		TaskId task2 = taskPool.addFinalized(new MergeSortTask(data, scratch, right, rightEnd, workCount));
		TaskId task3 = taskPool.addFinalized(new DepdencyTask(task1, task2));
		TaskId task4 = taskPool.addFinalized(new MergeTask(data, scratch, left, leftEnd, right, rightEnd), 0, task3);

		// Now task4 is depends on task3 while task3 will waits for task1 and task2

		taskPool.wait(task4);
	}
}

TEST(TaskPoolMultiThreadMergeSortTest)
{
	TaskPool taskPool;
	taskPool.init(3);

	Array<int> data(testSize), scratch(testSize);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	StopWatch stopWatch;
	float begin = stopWatch.getFloat();
	mergeSortMultiThread(&data[0], &scratch[0], 0, data.size(), taskPool);
	float dt =  stopWatch.getFloat() - begin;

	if(benchmark)
		printf("Time for MultiThreadMergeSort: %f\n", dt);

	for(rhuint i=1; i<data.size(); ++i)
		CHECK(data[i-1] <= data[i]);
}
