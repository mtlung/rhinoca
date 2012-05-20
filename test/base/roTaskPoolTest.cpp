#include "pch.h"
#include "../../src/common.h"
#include "../../roar/base/roTaskPool.h"
#include <math.h>

using namespace ro;

namespace {

class MyTask : public Task
{
public:
	MyTask(int id=0) : _id(id) {}
	override void run(TaskPool* taskPool)
	{
		float val =0.1f;
		for(roSize i=0; i<10000; ++i)
			val = sinf(val);

		(void)val;
//		printf("Task: %d, thread: %d, val: %f\n", _id, TaskPool::threadId(), val);

		delete this;
	}
	int _id;
};

class TaskPoolTest
{
public:
};

}	// namespace

TEST_FIXTURE(TaskPoolTest, empty) { TaskPool taskPool; }

TEST_FIXTURE(TaskPoolTest, singleThreadBasic)
{
	{	// Cleanup of task on taskPool destructor
		TaskPool taskPool;
		TaskId t = taskPool.beginAdd(new MyTask);
		taskPool.finishAdd(t);
	}

	{	// Cleanup of tasks on taskPool destructor
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);
		taskPool.finishAdd(t1);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		taskPool.finishAdd(t2);
	}
}

TEST_FIXTURE(TaskPoolTest, singleThreadDependency)
{
	{	// Task2 depends on task1, task1 will be run before task2
		TaskPool taskPool;
		TaskId t1 = taskPool.addFinalized(new MyTask);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		taskPool.dependsOn(t2, t1);
		taskPool.finishAdd(t2);

		taskPool.wait(t2);
	}

	{	// Task2 depends on task1, while task1 was already finished
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);
		taskPool.finishAdd(t1);

		taskPool.wait(t1);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		// The id 't1' is already become invalid, but it's not a problem since we sure task1 is already finished
		taskPool.dependsOn(t2, t1);
		taskPool.finishAdd(t2);
	}

	{	// Task1 depends on task2
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask(0));

		TaskId t2 = taskPool.beginAdd(new MyTask(1));
		taskPool.finishAdd(t2);

		taskPool.dependsOn(t1, t2);
		taskPool.finishAdd(t1);

		for(int i=0; i<10; ++i)
			taskPool.addFinalized(new MyTask(i+2), 0, 0, 0);

		while(taskPool.taskCount())
			taskPool.doSomeTask();
	}
}

TEST_FIXTURE(TaskPoolTest, singleThreadChild)
{
	{	// Task2 is child of task1
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		taskPool.addChild(t1, t2);
		taskPool.finishAdd(t2);

		taskPool.finishAdd(t1);

		taskPool.wait(t1);	// t2 will force to wait also
	}

	{	// Same of above but wait for t2
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		taskPool.addChild(t1, t2);
		taskPool.finishAdd(t2);

		taskPool.finishAdd(t1);

		taskPool.wait(t2);
	}

	{	// Task1 is child of task2
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);

		TaskId t2 = taskPool.beginAdd(new MyTask);
		taskPool.addChild(t2, t1);
		taskPool.finishAdd(t2);

		taskPool.finishAdd(t1);
	}
}

// This test aim to stress the task pool with dependency
TEST_FIXTURE(TaskPoolTest, dependencyDeadLock)
{
	class DummyTask : public Task
	{
	public:
		override void run(TaskPool* taskPool)
		{
//			printf("%u %u\n", thisId, TaskPool::threadId());
		}

		TaskId thisId;
		TaskId parent;
		TaskId depend;
	};

	TaskPool taskPool;
	taskPool.init(3);
	DummyTask tasks[1000];

	for(roSize i=0; i<COUNTOF(tasks); ++i)
	{
		tasks[i].thisId = 0;
		tasks[i].parent = 0;

		// Task with bigger index depends on task with smaller index
		tasks[i].depend = (i == 0) ? 0 : rand() % i;

		tasks[i].thisId = taskPool.beginAdd(&tasks[i]);
		taskPool.dependsOn(tasks[i].thisId, tasks[i].depend);
		taskPool.addChild(tasks[i].parent, tasks[i].thisId);
	}

	for(roSize i=0; i<COUNTOF(tasks); ++i) {
		taskPool.finishAdd(tasks[i].thisId);
	}

	taskPool.waitAll();
}
