#include "pch.h"
#include "../src/taskpool.h"
#include <math.h>
namespace {

class MyTask : public Task
{
public:
	MyTask(int id=0) : _id(id) {}
	virtual void run(TaskPool* taskPool)
	{
/*		float val =0.1f;
		for(int i=0; i<10000; ++i)
			val = sinf(val);

		printf("Task: %d, thread: %d, val: %f\n", _id, TaskPool::threadId(), val);*/
		printf("Task: %d, thread: %d\n", _id, TaskPool::threadId());

		delete this;
	}
	int _id;
};

class TaskPoolTest
{
public:
//	TaskPool taskPool;
};

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
/*	{	// Task2 depends on task1, task1 will be run before task2
		TaskPool taskPool;
		TaskId t1 = taskPool.beginAdd(new MyTask);
		taskPool.finishAdd(t1);

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
	}*/

	{	// Task1 depends on task2
		TaskPool taskPool;
		taskPool.init(4);
		TaskId t1 = taskPool.beginAdd(new MyTask(0));

		TaskId t2 = taskPool.beginAdd(new MyTask(1));
		taskPool.finishAdd(t2);

		taskPool.dependsOn(t1, t2);
		taskPool.finishAdd(t1);

		for(int i=0; i<10000; ++i) {
			taskPool.finishAdd(taskPool.beginAdd(new MyTask(i+2)));
		}

		while(taskPool.taskCount()) {
			taskPool.doSomeTask();
		}
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

}	// namespace
