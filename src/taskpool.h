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
	/// the Task is not going to reuse.
	virtual void run(TaskPool* taskPool) = 0;

	/// When there are idling thread in the TaskPool, it may request
	/// a Task to off load part of it's work to another new Task
	virtual bool offload(TaskPool* taskPool) { return false; }

	void* operator new(size_t size) { return rhinoca_malloc(size); }
	void operator delete(void* ptr) { rhinoca_free(ptr); }

protected:
	/// Put the task back to the pool, usefull when the task cannot complete immediatly.
	/// Make sure you won't delete 'this' in 'run()'
	void reSchedule();

private:
	friend class TaskPool;
	void* _proxy;	/// For use with reSchedule()
};	// Task

typedef rhuint TaskId;

class TaskPool
{
protected:
	class TaskItem;
	friend class Task;

public:
	TaskPool();
	~TaskPool();

// Operations
	void init(rhuint threadCount);

	/// You can addChild() and dependsOn() in-between beginAdd() and finishAdd()
	/// The task will begin to process as soon as possible
	TaskId beginAdd(Task* task, int affinity=0);

	/// A task is consider completed only if all it's children are completed
	void addChild(TaskId parent, TaskId child);

	/// A task cannot be start until it's depending task completes
	void dependsOn(TaskId src, TaskId on);

	/// The task will not being it's physical run until finishAdd() invoked
	void finishAdd(TaskId id);

	/// Add the task, set the properties and finish the add, all in a single function call
	TaskId addFinalized(Task* task, TaskId parent=0, TaskId dependency=0, int affinity=0);

	/// @note The TaskId may be reused for another task that you
	/// were NOT waiting for, but that's not the problem since the
	/// task should be finished anyway.
	void wait(TaskId id);

	/// Check if a task is finished.
	bool isDone(TaskId id);

	/// If you know your thread have some idle time,
	/// call this function to help consuming the task queue
	void doSomeTask();

	/// Return false if the TaskPool is going to shutdown
	bool keepRun() const;

	/// Callback to invoke when a task finish
	typedef void (*Callback)(TaskPool* taskPool, void* userData);
	void addCallback(TaskId id, Callback callback, void* userData, int affinity);

	static void sleep(int milliSeconds);

// Attributes
	/// To get the current thread id, which is use for setting affinity
	static int threadId();

	rhuint taskCount() const { return taskList.count; }

	int mainThreadId() const { return _mainThreadId; }

protected:
	class TaskProxy;

	friend void _run(TaskPool*);
	void _doTask(TaskProxy* id, int threadId);

	void _wait(TaskProxy* id, int threadId);

	TaskProxy* _findProxyById(TaskId id);

	/// Hold the TaskProxy such that it's member will not reset even
	/// the task has been finished and prevent the proxy from being reused.
	void _retainTask(TaskProxy* p);

	/// If the retain count reach zero, it means absolutly no one (potentially)
	/// intrested in the task any more, the proxy can be safely reused.
	void _releaseTask(TaskProxy* p);

	void _removeOpenTask(TaskProxy* p);

	void _addPendingTask(TaskProxy* p);

	// Remove pending task from the front
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
	TaskProxy* _openTasks;			///< Tasks which are not completed yet.
	TaskProxy* _pendingTasksHead;	///< Tasks which are not assigned to any worker yet, head of link list.
	TaskProxy* _pendingTasksTail;	///< Tasks which are not assigned to any worker yet, tail of link list.

	rhuint _threadCount;
	rhuint* _threadHandles;

	int _mainThreadId;

	mutable Mutex mutex;
};	// TaskPool

#endif	// __TASKPOOL_H__
