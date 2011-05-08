#include "pch.h"
#include "taskpool.h"
#include "platform.h"

// Inspired by BitSquid engine:
// http://bitsquid.blogspot.com/2010/03/task-management-practical-example.html

#define DEBUG_PRINT 0

class TaskPool::TaskProxy
{
public:
	TaskProxy();

	void* operator new(size_t size) { return rhinoca_malloc(size); }
	void operator delete(void* ptr) { rhinoca_free(ptr); }

	volatile TaskId id;		///< 0 for invalid id
	Task* task;				///< Once complete it will set to NULL
	bool finalized;			///< Attributes like dependency, affinity, parent cannot be set after the task is finalized
	bool isWaiting;			///< Indicate this task is already waiting to finish
	int affinity;
	TaskPool* taskPool;
	TaskProxy* parent;		///< A task is consider completed only if all it's children are completed.
	TaskProxy* dependency;	///< This task cannot be start until the depending task completes.
	TaskId dependencyId;	///< The dependency valid only if dependency->id == dependencyId
	int openChildCount;		///< When a task completes, it reduces the openChildCount of it's parent. When this figure reaches zero, the work is completed.

	TaskProxy* nextFree;
	TaskProxy* nextOpen, *prevOpen;
	TaskProxy* nextPending, *prevPending;
};	// TaskProxy

TaskPool::TaskProxy::TaskProxy()
	: id(0)
	, task(NULL)
	, finalized(false), isWaiting(false)
	, affinity(0)
	, taskPool(NULL)
	, parent(NULL)
	, dependency(NULL), dependencyId(0)
	, openChildCount(0)
	, nextFree(NULL)
	, nextOpen(NULL), prevOpen(NULL)
	, nextPending(NULL), prevPending(NULL)
{
}

TaskPool::TaskList::TaskList()
	: count(0)
	, idCounter(0)
	, freeBegin(NULL)
{
}

TaskPool::TaskList::~TaskList()
{
	ASSERT(count == 0);
	while(freeBegin) {
		TaskProxy* next = freeBegin->nextFree;
		ASSERT(!freeBegin->task);
		delete freeBegin;
		freeBegin = next;
	}
}

TaskPool::TaskProxy* TaskPool::TaskList::alloc()
{
	++count;
	++idCounter;

	if(!freeBegin)
		freeBegin = new TaskProxy;

	TaskProxy* ret = freeBegin;
	freeBegin = ret->nextFree;
	ret->nextFree = NULL;

	ret->id = idCounter;
	ASSERT(ret->task == NULL);

	return ret;
}

void TaskPool::TaskList::free(TaskProxy* id)
{
#ifndef NDEBUG
	for(TaskProxy* p = freeBegin; p; p = p->nextFree)
		ASSERT(p != id);
#endif

	*id = TaskProxy();
	id->nextFree = freeBegin;
	freeBegin = id;
	ASSERT(count > 0);
	--count;
}

TaskPool::TaskPool()
	: _keepRun(true)
	, _openTasks(NULL)
	, _pendingTasksHead(NULL), _pendingTasksTail(NULL)
	, _threadCount(0)
	, _threadHandles(NULL)
	, _mainThreadId(TaskPool::threadId())
{
	_pendingTasksHead = new TaskProxy();
	_pendingTasksTail = new TaskProxy();

	_pendingTasksHead->nextPending = _pendingTasksTail;
	_pendingTasksTail->prevPending = _pendingTasksHead;
}

TaskPool::~TaskPool()
{
	int tId = threadId();
	ScopeLock lock(mutex);

	_keepRun = false;
	while(_openTasks)
		_wait(_openTasks, tId);

	for(rhuint i=0; i<_threadCount; ++i) {
		ScopeUnlock unlock(mutex);

#ifdef RHINOCA_WINDOWS
		HANDLE h = reinterpret_cast<HANDLE>(_threadHandles[i]);
		VERIFY(::WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0);
		VERIFY(::CloseHandle(h));
#else
		pthread_t h = reinterpret_cast<pthread_t>(_threadHandles[i]);
		VERIFY(::pthread_join(h, NULL) == 0);
		::pthread_detach(h);
#endif
	}

	rhdelete(_threadHandles);

	delete _pendingTasksHead;
	delete _pendingTasksTail;
}

void TaskPool::sleep(int ms)
{
#ifdef RHINOCA_WINDOWS
		::Sleep(DWORD(ms));
#else
		::usleep(useconds_t(ms * 1000));
#endif
}

void _run(TaskPool* pool)
{
	while(pool->keepRun()) {
		pool->doSomeTask();

		// TODO: Use condition variable
		TaskPool::sleep(1);
	}
}

#ifdef RHINOCA_WINDOWS
static DWORD WINAPI _threadFunc(LPVOID p)
{
#else
static void* _threadFunc(void* p) {
#endif
	TaskPool* pool = reinterpret_cast<TaskPool*>(p);
	_run(pool);

	return 0;
}

void TaskPool::init(rhuint threadCount)
{
	ASSERT(!_threadHandles);
	_threadCount = threadCount;
	_threadHandles = rhnew<rhuint>(threadCount);

	for(rhuint i=0; i<_threadCount; ++i) {
#ifdef RHINOCA_WINDOWS
		_threadHandles[i] = reinterpret_cast<rhuint>(::CreateThread(NULL, 0, &_threadFunc, this, 0, NULL));
#else
		VERIFY(::pthread_create(reinterpret_cast<pthread_t*>(&_threadHandles[i]), NULL, &_threadFunc, this) == 0);
#endif
	}
}

TaskId TaskPool::beginAdd(Task* task, int affinity)
{
	ScopeLock lock(mutex);

	TaskProxy* proxy = taskList.alloc();
	proxy->taskPool = this;
	proxy->task = task;
	proxy->affinity = affinity;

	if(_openTasks) _openTasks->prevOpen = proxy;
	proxy->nextOpen = _openTasks;
	_openTasks = proxy;

	_addPendingTask(proxy);

	_retainTask(proxy);	// Don't let this task to finish, before we call finishAdd()

	return proxy->id;
}

void TaskPool::addChild(TaskId parent, TaskId child)
{
	ScopeLock lock(mutex);

	TaskProxy* parentProxy = _findProxyById(parent);
	TaskProxy* childProxy = _findProxyById(child);

	ASSERT(parentProxy && !parentProxy->finalized && "Parameter 'parent' has already finalized");
	ASSERT(childProxy && !childProxy->finalized && "Parameter 'child' has already finalized");
	ASSERT(!childProxy->parent && "The given child task is already under");

	_retainTask(parentProxy);	// Paired with releaseTask() in _doTask()
	childProxy->parent = parentProxy;
}

void TaskPool::dependsOn(TaskId src, TaskId on)
{
	ScopeLock lock(mutex);

	TaskProxy* srcProxy = _findProxyById(src);
	TaskProxy* onProxy = _findProxyById(on);

	ASSERT(srcProxy && !srcProxy->finalized && "Parameter 'src' has already finalized");

	srcProxy->dependency = onProxy;
	srcProxy->dependencyId = on;
}

void TaskPool::finishAdd(TaskId id)
{
	ScopeLock lock(mutex);

	if(TaskProxy* p = _findProxyById(id)) {
		ASSERT(!p->finalized && "Please call finishAdd() only once");
		p->finalized = true;
		_releaseTask(p);
	}
}

TaskId TaskPool::addFinalized(Task* task, TaskId parent, TaskId dependency, int affinity)
{
	ScopeLock lock(mutex);

	TaskProxy* proxy = taskList.alloc();
	proxy->taskPool = this;
	proxy->task = task;
	proxy->affinity = affinity;
	proxy->finalized = true;

	if(parent != 0) {
		TaskProxy* parentProxy = _findProxyById(parent);
		ASSERT(parentProxy && !parentProxy->finalized && "Parameter 'parent' has already finalized");
		_retainTask(parentProxy);	// Paired with releaseTask() in _doTask()
		proxy->parent = parentProxy;
	}

	if(dependency != 0) {
		TaskProxy* depProxy = _findProxyById(dependency);
		proxy->dependency = depProxy;
		proxy->dependencyId = dependency;
	}

	if(_openTasks) _openTasks->prevOpen = proxy;
	proxy->nextOpen = _openTasks;
	_openTasks = proxy;

	_addPendingTask(proxy);

	return proxy->id;
}

bool TaskPool::keepRun() const
{
	return _keepRun;
}

int TaskPool::threadId()
{
#ifdef RHINOCA_WINDOWS
	return ::GetCurrentThreadId();
#else
	return int(::pthread_self());
#endif
}

// NOTE: Recursive and re-entrant
void TaskPool::wait(TaskId id)
{
	int tId = threadId();
	ScopeLock lock(mutex);
	if(TaskProxy* p = _findProxyById(id)) {
		_removePendingTask(p);
		_wait(p, tId);
	}
}

#if DEBUG_PRINT
static int _debugWaitCount = 0;
static const int _debugMaxIndent = 10;
static const char _debugIndent[_debugMaxIndent+1] = "          ";
#endif

void TaskPool::_wait(TaskProxy* p, int tId)
{
	ASSERT(mutex.isLocked());
	if(!p) return;

#if DEBUG_PRINT
	printf("%sBegin _wait(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
	++_debugWaitCount;
#endif

	TaskId id = p->id;
	p->isWaiting = true;
	if(p->affinity == 0 || p->affinity == tId) {
		_doTask(p, tId);
		if(p->id == id)
			goto DoOtherTasks;
	}
	else {
		// Do other tasks until the task in question was finish
		// When the task get finished, p->task becomes null
	DoOtherTasks:
		// If p->id not equals to id, it means the task is already finished long ago, and the proxy get reused.
		while(p->id == id) {
			TaskProxy* p2 = _pendingTasksHead->nextPending;

			// Search for a task which it's dependency (if any) is not already running by the upper callstack, to prevent dead lock
			while(p2->dependency && !p2->dependency->task) {
				p2 = p2->nextPending;
				continue;
			}

			if(p2 != _pendingTasksTail)
				_doTask(p2, tId);
			else {
				// If no more task can do we sleep for a while
				// NOTE: Must release mutex to avoid deal lock
				ScopeUnlock unlock(mutex);
				TaskPool::sleep(1);
			}
		}
	}

#if DEBUG_PRINT
	--_debugWaitCount;
	printf("%sEnd _wait(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif
}

bool TaskPool::isDone(TaskId id)
{
	ScopeLock lock(mutex);
	return _findProxyById(id) == NULL;
}

void TaskPool::doSomeTask()
{
	int tId = threadId();
	ScopeLock lock(mutex);

	TaskProxy* p = _pendingTasksHead->nextPending;
	while(p != _pendingTasksTail) {
		TaskProxy* next = p->nextPending;

		// Don't pick a task to do if it's dependency is waiting for finish, to prevent deal lock
		const bool isAlreadyWaiting = p->dependency && p->dependency->isWaiting;

		if(p->affinity == 0 || p->affinity == tId && !isAlreadyWaiting) {
			// NOTE: After _doTask(), the 'next' pointer becomes invalid
			_doTask(p, tId);

			// NOTE: Each time _doTask() is invoked, we start to search task
			// at the beginning, to avoid the problem of 'next' become invalid,
			// may be in-efficient, but easier to implement.
			p = _pendingTasksHead->nextPending;
		}
		else
			p = next;
	}
}

// NOTE: Recursive and re-entrant
void TaskPool::_doTask(TaskProxy* p, int tId)
{
	ASSERT(mutex.isLocked());
	
	if(!p || !p->task) return;

#if DEBUG_PRINT
	printf("%sBegin _doTask(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif

	Task* task = p->task;
	p->task = NULL;

	_removePendingTask(p);

	// NOTE: _wait() may trigger many things, therefore we need to _retainTask() here
	_retainTask(p);

	if(TaskProxy* dep = p->dependency) {
		if(dep->id == p->dependencyId)
			_wait(dep, tId);
	}

	task->_proxy = p;

	{	ScopeUnlock unlock(mutex);
		task->run(this);
		task = NULL;	// The task may be deleted, never use the pointer up to this point
	}

#if DEBUG_PRINT
	printf("%sEnd _doTask(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif

	if(TaskProxy* parent = p->parent)
		_releaseTask(parent);

	_releaseTask(p);
}

TaskPool::TaskProxy* TaskPool::_findProxyById(TaskId id)
{
	ASSERT(mutex.isLocked());

	if(!_openTasks) return NULL;
	
	// Search for the id in the open list
	TaskProxy* proxy = _openTasks;
	while(proxy) {
		if(proxy->id == id) return proxy;
		proxy = proxy->nextOpen;
	}

	return NULL;
}

void TaskPool::_retainTask(TaskProxy* p)
{
	p->openChildCount++;
}

void TaskPool::_releaseTask(TaskProxy* p)
{
	ASSERT(mutex.isLocked());
	p->openChildCount--;
	if(p->openChildCount == 0 && !p->task)
		_removeOpenTask(p);
}

void TaskPool::_removeOpenTask(TaskProxy* p)
{
	ASSERT(mutex.isLocked());
	ASSERT(!p->task);
	if(p->prevOpen) p->prevOpen->nextOpen = p->nextOpen;
	if(p->nextOpen) p->nextOpen->prevOpen = p->prevOpen;
	if(p == _openTasks) _openTasks = p->nextOpen;
	taskList.free(p);
}

void TaskPool::_addPendingTask(TaskProxy* p)
{
	ASSERT(mutex.isLocked());

	static const bool addOnTail = true;

	if(addOnTail) {
		TaskProxy* beg = _pendingTasksHead->nextPending;
		p->nextPending = beg;
		p->prevPending = _pendingTasksHead;
		beg->prevPending = p;
		_pendingTasksHead->nextPending = p;
	}
	else {
		TaskProxy* end = _pendingTasksTail->prevPending;
		p->prevPending = end;
		p->nextPending = _pendingTasksTail;
		end->nextPending = p;
		_pendingTasksTail->prevPending = p;
	}
}

void TaskPool::_removePendingTask(TaskProxy* p)
{
	ASSERT(mutex.isLocked());
	p->prevPending->nextPending = p->nextPending;
	p->nextPending->prevPending = p->prevPending;
	p->prevPending = p->nextPending = NULL;
}

// NOTE: This function is purely build on top of other public interface of TaskPool ^.^
void TaskPool::addCallback(TaskId id, Callback callback, void* userData, int affinity)
{
	class CallbackTask : public Task
	{
	public:
		virtual void run(TaskPool* taskPool)
		{
			callback(taskPool, userData);
			delete this;
		}
		void* userData;
		TaskPool::Callback callback;
	};

	CallbackTask* t = new CallbackTask;
	t->callback = callback;
	t->userData = userData;
	addFinalized(t, 0, id, affinity);
}

void Task::reSchedule()
{
	TaskPool::TaskProxy* p = reinterpret_cast<TaskPool::TaskProxy*>(this->_proxy);

	ScopeLock lock(p->taskPool->mutex);

	p->task = this;
	p->taskPool->_addPendingTask(p);

#if DEBUG_PRINT
	printf("%sreSchedule(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif
}
