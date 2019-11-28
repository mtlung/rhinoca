#ifndef __roTaskPool_h__
#define __roTaskPool_h__

#include "roCondVar.h"
#include <functional>

namespace ro {

class TaskPool;

typedef roUint32 TaskId;
typedef roPtrInt ThreadId;

class Task
{
public:
	Task() : _proxy(nullptr) {}
	virtual ~Task() {}

	/// Derived class will do actual work in this function.
	/// User should call 'delete this;' in this function if
	/// the Task is not going to reuse.
	virtual void run(TaskPool* taskPool) = 0;

	/// When there are idling thread in the TaskPool, it may request
	/// a Task to off load part of it's work to another new Task
	virtual bool offload(TaskPool* taskPool) { (void)taskPool; return false; }

protected:
	/// Put the task back to the pool, useful when the task cannot complete immediately.
	/// Make sure you won't delete 'this' in 'run()'
	/// The re-scheduled task can be suspended and resume again using TaskPool::suspend(id, false);
	void reSchedule(bool suspend=false);
	void reSchedule(bool suspend, ThreadId affinity);

private:
	friend class TaskPool;
	void* _proxy;	/// For use with reSchedule()
};	// Task


// ----------------------------------------------------------------------

class TaskPool
{
protected:
	class TaskItem;
	friend class Task;

public:
	TaskPool();
	~TaskPool();

	typedef void (*ThreadFunction)(TaskPool*);

// Operations
	void init(roSize threadCount, ThreadFunction threadFunction= NULL);

	/// You can addChild() and dependsOn() in-between beginAdd() and finishAdd()
	/// The task will begin to process as soon as possible
	/// \param affinity
	///		= 0 : any thread can run this task
	///		= threadId : the only thread that can run this task
	///		= ~threadId : the only thread that can NOT run this task
	TaskId beginAdd(Task* task, ThreadId affinity=0);
	TaskId beginAdd(const std::function<void()>& func, ThreadId affinity=0);

	/// A task is consider completed only if all it's children are completed
	void addChild(TaskId parent, TaskId child);

	/// A task cannot be start until it's depending task completes
	void dependsOn(TaskId src, TaskId on);

	/// The task will not being it's physical run until finishAdd() invoked
	void finishAdd(TaskId id);

	/// Add the task, set the properties and finish the add, all in a single function call
	TaskId addFinalized(Task* task, TaskId parent=0, TaskId dependency=0, ThreadId affinity=0);
	TaskId addFinalized(const std::function<void()>& func, TaskId parent=0, TaskId dependency=0, ThreadId affinity=0);

	/// @note The TaskId may be reused for another task that you
	/// were NOT waiting for, but that's not the problem since the
	/// task should be finished anyway.
	void wait(TaskId id);

	/// A task will not be pick up by the TaskPool to run, if it's suspended.
	/// For more deterministic behavior, a suspension counter is used,
	/// such that the order of calling of suspend() and resume() is irrelevant.
	/// @note If the task is already running, it will not preempted.
	void suspend(TaskId id);
	void resume(TaskId id);

	/// Check if a task is finished.
	bool isDone(TaskId id);

	/// If you know your thread have some idle time,
	/// call this function to help consuming the task queue
	/// timeout = 0 for no timeout
	void doSomeTask(float timeout=0);

	/// Return false if the TaskPool is going to shutdown
	bool keepRun() const;

	/// Wait till they are finished
	void waitAll();

	/// Callback to invoke when a task finish. It also get triggered
	/// if the task is already done at the time you invoke addCallback()
	typedef void (*Callback)(TaskPool* taskPool, void* userData);
	void addCallback(TaskId id, Callback callback, void* userData, ThreadId affinity);

	static void sleep(int milliSeconds);

	/// This function is better than sleep(0)
	static void yield();

	/// For use in custom thread function
	void waitForTaskAvailable(unsigned timeoutInMs=unsigned(-1));

// Attributes
	/// To get the current thread id, which is use for setting affinity
	static ThreadId threadId();

	roSize taskCount() const { return taskList.count; }
	roSize threadCount() const { return _threadCount; }

	ThreadId mainThreadId() const { return _mainThreadId; }
	ThreadFunction threadFunction() const { return _threadFunction; }

protected:
	class TaskProxy;

	friend void defaultThreadFunction(TaskPool*);
	void _doTask(TaskProxy* id, ThreadId threadI);

	void _wait(TaskProxy* id, ThreadId threadId);

	TaskProxy* _findProxyById(TaskId id);

	/// Hold the TaskProxy such that it's member will not reset even
	/// the task has been finished and prevent the proxy from being reused.
	void _retainTask(TaskProxy* p);

	/// If the retain count reach zero, it means absolutely no one (potentially)
	/// interested in the task any more, the proxy can be safely reused.
	void _releaseTask(TaskProxy* p);

	void _removeOpenTask(TaskProxy* p);

	void _addPendingTask(TaskProxy* p);

	// Remove pending task from the front
	void _removePendingTask(TaskProxy* p);

	bool _matchAffinity(TaskProxy* p, ThreadId threadId);

	bool _hasOutstandingDependency(TaskProxy* p);

	void _waitForTaskAvailableNoLock(unsigned timeoutInMs);

	class TaskList
	{
	public:
		TaskList();
		~TaskList();

		TaskProxy* alloc();

		void free(TaskProxy* id);

		roSize count;
		TaskId idCounter;
		TaskProxy* freeBegin;	///< Point to the first free entry
	};	// TaskList

	TaskList taskList;
	ThreadFunction _threadFunction;

	bool _keepRun;
	TaskProxy* _openTasks;			///< Tasks which are not completed yet.
	TaskProxy* _pendingTasksHead;	///< Tasks which are not assigned to any worker yet, head of link list.
	TaskProxy* _pendingTasksTail;	///< Tasks which are not assigned to any worker yet, tail of link list.
	roSize _pendingTaskCount;

	roSize _threadCount;
	roSize _workerWaitCount;
	void** _threadHandles;

	ThreadId _mainThreadId;

	float _avgTaskPerSecond;		///< We keep some statistic for making doSomeTask() more efficient

	mutable CondVar condVar;
};	// TaskPool

}	// namespace ro

#endif	// __roTaskPool_h__
