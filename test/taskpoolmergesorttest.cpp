#include "pch.h"
#include "../src/taskpool.h"
#include "../src/timer.h"
#include "../src/vector.h"
#include <stdlib.h>

static const bool benchmark = false;
static const rhuint testSize = benchmark ? 10000000 : 1000;

/// scratch is a memory buffer having the same size as data
static void merge(int* data, int* scratch, int left, int leftEnd, int right, int rightEnd)
{
	// The merge operation is much faster with a scratch memory buffer
	if(scratch) {
		int begin = left;
		int end = rightEnd;
		for(int i=begin; i<end; ++i) {
			if(right >= rightEnd || (left < leftEnd && data[left] <= data[right]))
				scratch[i] = data[left++];
			else
				scratch[i] = data[right++];
		}
		for(int i=begin; i<end; ++i)
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

static void mergeSortSingleThread(int* data, int* scratch, int begin, int end)
{
	int middle = (end - begin) / 2;
	if(middle == 0) return;

	int left = begin;
	int leftEnd = begin + middle;
	int right = leftEnd;
	int rightEnd = end;

	mergeSortSingleThread(data, scratch, left, leftEnd);

	mergeSortSingleThread(data, scratch, right, rightEnd);

	merge(data, scratch, left, leftEnd, right, rightEnd);
}

TEST(TaskPoolSingleThreadMergeSortTest)
{
	Vector<int> data(testSize), scratch(testSize);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	Timer timer;
	float begin = timer.seconds();
	mergeSortSingleThread(&data[0], &scratch[0], 0, data.size());
	float dt =  timer.seconds() - begin;

	if(benchmark)
		printf("Time for SingleThreadMergeSort: %f\n", dt);

	for(rhuint i=1; i<data.size(); ++i)
		CHECK(data[i-1] <= data[i]);
}

static void mergeSortMultiThread(int* data, int* scratch, int begin, int end, TaskPool& taskPool, int workCount=1)
{
	int middle = (end - begin) / 2;
	if(middle == 0) return;

	int left = begin;
	int leftEnd = begin + middle;
	int right = leftEnd;
	int rightEnd = end;

	class MergeSortTask : public Task
	{
	public:
		MergeSortTask(int* data, int* scratch, int begin, int end, int workCount)
			: _data(data), _scratch(scratch), _begin(begin), _end(end), _workCount(workCount)
		{}

		override void run(TaskPool* taskPool)
		{
			mergeSortMultiThread(_data, _scratch, _begin, _end, *taskPool, _workCount * 2);
			delete this;
		}

		int *_data, *_scratch, _begin, _end, _workCount;
	};

	class MergeTask : public Task
	{
	public:
		MergeTask(int* data, int* scratch, int left, int leftEnd, int right, int rightEnd)
			: _data(data), _scratch(scratch), _left(left), _leftEnd(leftEnd), _right(right), _rightEnd(rightEnd)
		{}

		override void run(TaskPool* taskPool)
		{
			merge(_data, _scratch, _left, _leftEnd, _right, _rightEnd);
			delete this;
		}

		int *_data, *_scratch, _left, _leftEnd, _right, _rightEnd;
	};

	class DepdencyTask : public Task
	{
	public:
		DepdencyTask(TaskId t1, TaskId t2)
			: _t1(t1), _t2(t2)
		{}

		override void run(TaskPool* taskPool)
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

	Vector<int> data(testSize), scratch(testSize);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	Timer timer;
	float begin = timer.seconds();
	mergeSortMultiThread(&data[0], &scratch[0], 0, data.size(), taskPool);
	float dt =  timer.seconds() - begin;

	if(benchmark)
		printf("Time for MultiThreadMergeSort: %f\n", dt);

	for(rhuint i=1; i<data.size(); ++i)
		CHECK(data[i-1] <= data[i]);
}
