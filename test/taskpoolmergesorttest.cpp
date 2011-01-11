#include "pch.h"
#include "../src/taskpool.h"
#include <stdlib.h>
#include <vector>

static void merge(int* data, int left, int leftEnd, int right, int rightEnd)
{
	// Insert the right data into correct location in the left
	for(;left < rightEnd && right < rightEnd; ++left) {
		int val = data[right];
		if(left >= right || val >= data[left]) continue;
		memmove(data + left + 1, data + left, sizeof(int) * (right - left));
		data[left] = val;
		++right;
	}
}

static void mergeSortSingleThread(int* data, int begin, int end)
{
	int middle = (end - begin) / 2;
	if(middle == 0) return;

	int left = begin;
	int leftEnd = begin + middle;
	int right = leftEnd;
	int rightEnd = end;

	mergeSortSingleThread(data, left, leftEnd);

	mergeSortSingleThread(data, right, rightEnd);

	merge(data, left, leftEnd, right, rightEnd);
}

TEST(TaskPoolSingleThreadMergeSortTest)
{
	std::vector<int> data(1000);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	mergeSortSingleThread(&data[0], 0, data.size());
}

static void mergeSortMultiThread(int* data, int begin, int end, TaskPool& taskPool, int workCount=1)
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
		MergeSortTask(int* data, int begin, int end, int workCount)
			: _data(data), _begin(begin), _end(end), _workCount(workCount)
		{}

		virtual void run(TaskPool* taskPool)
		{
			mergeSortMultiThread(_data, _begin, _end, *taskPool, _workCount * 2);
			delete this;
		}

		int *_data, _begin, _end, _workCount;
	};

	class MergeTask : public Task
	{
	public:
		MergeTask(int* data, int left, int leftEnd, int right, int rightEnd)
			: _data(data), _left(left), _leftEnd(leftEnd), _right(right), _rightEnd(rightEnd)
		{}

		virtual void run(TaskPool* taskPool)
		{
			merge(_data, _left, _leftEnd, _right, _rightEnd);
			delete this;
		}

		int *_data, _left, _leftEnd, _right, _rightEnd;
	};

	class DepdencyTask : public Task
	{
	public:
		DepdencyTask(TaskId t1, TaskId t2)
			: _t1(t1), _t2(t2)
		{}

		virtual void run(TaskPool* taskPool)
		{
			taskPool->wait(_t1);
			taskPool->wait(_t2);
			delete this;
		}

		TaskId _t1, _t2;
	};

	if(workCount == 4)
		mergeSortSingleThread(data, begin, end);
	else
	{
		TaskId task1 = taskPool.addFinalized(new MergeSortTask(data, left, leftEnd, workCount));
		TaskId task2 = taskPool.addFinalized(new MergeSortTask(data, right, rightEnd, workCount));
		TaskId task3 = taskPool.addFinalized(new DepdencyTask(task1, task2));
		TaskId task4 = taskPool.addFinalized(new MergeTask(data, left, leftEnd, right, rightEnd), 0, task3);

		// Now task4 is depends on task3 while task3 will waits for task1 and task2

		taskPool.wait(task4);
	}
}

TEST(TaskPoolMultiThreadMergeSortTest)
{
	TaskPool taskPool;
	taskPool.init(3);

	std::vector<int> data(1000);
	for(rhuint i=0; i<data.size(); ++i)
		data[i] = rand() % data.size();

	mergeSortMultiThread(&data[0], 0, data.size(), taskPool);

	data[0] = data[0];
}
