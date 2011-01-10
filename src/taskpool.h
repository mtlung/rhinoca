#ifndef __TASKPOOL_H__
#define __TASKPOOL_H__

#include "atomic.h"
#include "mutex.h"

class TaskPool;

class Task
{
public:
	virtual ~Task() {}

	/// Derived class will do actual work in this function.
	/// User should call 'delete this;' in this function if
	/// the Task is not going to resure.
	virtual void run(TaskPool* taskPool) = 0;

	/// When there are idling thread in the TaskPool, it may request
	/// a Task to off load part of it's work to another new Task
	virtual bool offload(TaskPool* taskPool) { return false; }
};	// Task

typedef rhuint TaskId;

class TaskPool
{
protected:
	class TaskItem;

public:
	TaskPool();
	~TaskPool();

	void init(rhuint threadCount);

	/// You can addChild() and dependsOn() in-between beginAdd() and finishAdd()
	TaskId beginAdd(Task* task);

	void finishAdd(TaskId id);

	void addChild(TaskId parent, TaskId child);

	void dependsOn(TaskId src, TaskId on);

	/// @note The TaskId may be reused for another task that you
	/// were NOT waiting for, but that's not the problem since the
	/// task should be finished anyway.
	void wait(TaskId id);

	/// If you know your thread have some idle time,
	/// call this function to help consuming the task queue
	void doSomeTask();

	/// Return false if the TaskPool is going to shutdown
	bool keepRun() const;

	/// To get the current thread id, which is use for setting affinity
	static int threadId();

	int taskCount() const { return taskList.count; }

protected:
	class TaskProxy;

	friend void _run(TaskPool*);
	void _doTask(TaskProxy* id);

	void _wait(TaskProxy* id);

	TaskProxy* _findProxyById(TaskId id);

	void _removeOpenTask(TaskProxy* p);

	void _removePendingTask(TaskProxy* p);

	class TaskList
	{
	public:
		TaskList();
		~TaskList();

		TaskProxy* alloc();

		void free(TaskProxy* id);

		AtomicInteger count;
		AtomicInteger idCounter;
		TaskProxy* freeBegin;	///< Point to the first free entry
	};	// TaskList

	TaskList taskList;

	bool _keepRun;
	TaskProxy* _openTasks;		///< Tasks which are not completed yet.
	TaskProxy* _pendingTasks;	///< Tasks which are not assigned to any worker yet.

	rhuint _threadCount;
	rhuint* _threadHandles;

	mutable Mutex mutex;
};	// TaskPool

#endif	// __TASKPOOL_H__
