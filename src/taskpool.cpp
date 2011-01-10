#include "pch.h"
#include "taskpool.h"
#include "platform.h"

// Inspired by BitSquid engine:
// http://bitsquid.blogspot.com/2010/03/task-management-practical-example.html

class TaskPool::TaskProxy
{
public:
	TaskProxy();

	TaskId id;				///< 0 for invalid id
	Task* task;				///< Once complete it will set to NULL
	bool finalized;			///< Attributes like dependency, affinity, parent cannot be set after the task is finalized
	int affinity;
	TaskProxy* parent;		///< A task is consider completed only if all it's child are completed.
	TaskProxy* dependency;	///< This task cannot be start until the depending task completes.
	int openChildCount;		///< When a task completes, it reduces the openChildCount of it's parent. When this figure reaches zero, the work is completed.

	TaskProxy* nextFree;
	TaskProxy* nextOpen, *prevOpen;
	TaskProxy* nextPending, *prevPending;
};	// TaskProxy

TaskPool::TaskProxy::TaskProxy()
	: task(NULL)
	, finalized(false)
	, affinity(0)
	, parent(NULL), dependency(NULL)
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

	ret->id = idCounter;
	ASSERT(ret->task == NULL);

	return ret;
}

void TaskPool::TaskList::free(TaskProxy* id)
{
	*id = TaskProxy();
	id->nextFree = freeBegin;
	freeBegin = id;
	--count;
}

TaskPool::TaskPool()
	: _keepRun(true)
	, _openTasks(NULL)
	, _pendingTasks(NULL)
	, _threadCount(0)
	, _threadHandles(NULL)
{
}

TaskPool::~TaskPool()
{
	ScopeLock lock(mutex);

	_keepRun = false;
	while(_openTasks)
		_wait(_openTasks);

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
}

static void _run(TaskPool* pool)
{
	while(pool->keepRun()) {
		pool->doSomeTask();

		// TODO: Use condition variable
#ifdef RHINOCA_WINDOWS
//		::Sleep(DWORD(1));
#else
		::usleep(useconds_t(1 * 1000));
#endif
	}
	::Sleep(DWORD(1));
}

#ifdef RHINOCA_WINDOWS
static DWORD WINAPI _threadFunc(LPVOID p)
{
#else
static void* _Run(void* p) {
#endif
	TaskPool* pool = reinterpret_cast<TaskPool*>(p);
	_run(pool);

	return 0;
}

void TaskPool::init(rhuint threadCount)
{
	_threadCount = threadCount;
	_threadHandles = rhnew<rhuint>(threadCount);

	for(rhuint i=0; i<_threadCount; ++i) {
#ifdef RHINOCA_WINDOWS
		_threadHandles[i] = reinterpret_cast<rhuint>(::CreateThread(NULL, 0, &_threadFunc, this, 0, NULL));
#else
		VERIFY(::pthread_create(&_threadHandles[i], NULL, &_threadFunc, this) == 0);
#endif
	}
}

TaskId TaskPool::beginAdd(Task* task)
{
	ScopeLock lock(mutex);

	TaskProxy* proxy = taskList.alloc();
	proxy->task = task;

	if(_openTasks) _openTasks->prevOpen = proxy;
	proxy->nextOpen = _openTasks;
	_openTasks = proxy;

	if(_pendingTasks) _pendingTasks->prevPending = proxy;
	proxy->nextPending = _pendingTasks;
	_pendingTasks = proxy;

	proxy->openChildCount = 1;	// Don't let this task to finish, before we call finishAdd()

	return proxy->id;
}

void TaskPool::finishAdd(TaskId id)
{
	ScopeLock lock(mutex);

	if(TaskProxy* p = _findProxyById(id)) {
		ASSERT(!p->finalized && "Please call finishAdd() only once");
		p->finalized = true;
		p->openChildCount--;

		if(p->openChildCount == 0 && !p->task)
			_removeOpenTask(p);
	}
}

void TaskPool::addChild(TaskId parent, TaskId child)
{
	ScopeLock lock(mutex);

	TaskProxy* parentProxy = _findProxyById(parent);
	TaskProxy* childProxy = _findProxyById(child);

	ASSERT(parentProxy && !parentProxy->finalized && "Call addChild() before calling finishAdd() on the parent");
	ASSERT(childProxy && !childProxy->finalized && "Call addChild() before calling finishAdd() on the child");
	ASSERT(!childProxy->parent && "The given child task is already under");

	parentProxy->openChildCount++;
	childProxy->parent = parentProxy;
}

void TaskPool::dependsOn(TaskId src, TaskId on)
{
	ScopeLock lock(mutex);

	TaskProxy* srcProxy = _findProxyById(src);
	TaskProxy* onProxy = _findProxyById(on);

	ASSERT(srcProxy && !srcProxy->finalized && "Call dependsOn() before calling finishAdd() on the src");

	srcProxy->dependency = onProxy;
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
	ScopeLock lock(mutex);
	_wait(_findProxyById(id));
}

void TaskPool::_wait(TaskProxy* p)
{
	ASSERT(mutex.isLocked());
	if(!p || !p->task) return;	// Already finished

	Task* taskToWait = p->task;
	int thisThreadAffinit = 0;
	if(p->affinity == -1 || p->affinity == thisThreadAffinit) {
		_doTask(p);
		if(p->openChildCount)
			goto DoOtherTasks;
	}
	else {
	DoOtherTasks:
		// Do other tasks until the task in question was finish
		// When the task get finished, p->task becomes null
		while(p->task == taskToWait || p->openChildCount) {
			_doTask(this->_pendingTasks);
		}
	}
}

void TaskPool::doSomeTask()
{
	ScopeLock lock(mutex);

	while(_pendingTasks)
		_doTask(_pendingTasks);
}

// NOTE: Recursive and re-entrant
void TaskPool::_doTask(TaskProxy* p)
{
	ASSERT(mutex.isLocked());
	ASSERT(p->task);

	Task* task = p->task;
	p->task = NULL;

	_removePendingTask(p);

	if(p->dependency)
		_wait(p->dependency);

	p->openChildCount++;
	{	ScopeUnlock unlock(mutex);
		task->run(this);
	}
	p->openChildCount--;

	if(TaskProxy* parent = p->parent) {
		parent->openChildCount--;
		if(parent->openChildCount == 0 && !parent->task)
			_removeOpenTask(parent);
	}

	if(p->openChildCount == 0)
		_removeOpenTask(p);
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

void TaskPool::_removeOpenTask(TaskProxy* p)
{
	if(p->prevOpen) p->prevOpen->nextOpen = p->nextOpen;
	if(p->nextOpen) p->nextOpen->prevOpen = p->prevOpen;
	if(p == _openTasks) _openTasks = p->nextOpen;
	taskList.free(p);
}

void TaskPool::_removePendingTask(TaskProxy* p)
{
	if(p->prevPending) p->prevPending->nextPending = p->nextPending;
	if(p->nextPending) p->nextPending->prevPending = p->prevPending;
	if(p == _pendingTasks) _pendingTasks = p->nextPending;
	p->prevPending = p->nextPending = NULL;
}
