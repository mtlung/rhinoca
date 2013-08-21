#include "pch.h"
#include "roTaskPool.h"
#include "roCpuProfiler.h"
#include "roMemory.h"
#include "../platform/roPlatformHeaders.h"

// Inspired by BitSquid engine:
// http://bitsquid.blogspot.com/2010/03/task-management-practical-example.html

#define DEBUG_PRINT 0

namespace ro {

static DefaultAllocator _allocator;

class TaskPool::TaskProxy
{
public:
	TaskProxy();

	volatile TaskId id;		///< 0 for invalid id
	Task* task;				///< Once complete it will set to NULL
	bool finalized;			///< Attributes like dependency, affinity, parent cannot be set after the task is finalized
	ThreadId affinity;
	TaskPool* taskPool;
	TaskProxy* parent;		///< A task is consider completed only if all it's children are completed.
	TaskProxy* dependency;	///< This task cannot be start until the depending task completes.
	TaskId dependencyId;	///< The dependency valid only if dependency->id == dependencyId
	int openChildCount;		///< When a task completes, it reduces the openChildCount of it's parent. When this figure reaches zero, the work is completed.
	int suspensionCount;	///< Keep how many times the task is suspended.

	TaskProxy* nextFree;
	TaskProxy* nextOpen, *prevOpen;
	TaskProxy* nextPending, *prevPending;
};	// TaskProxy

TaskPool::TaskProxy::TaskProxy()
	: id(0)
	, task(NULL)
	, finalized(false)
	, affinity(0)
	, taskPool(NULL)
	, parent(NULL)
	, dependency(NULL), dependencyId(0)
	, openChildCount(0)
	, suspensionCount(0)
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
	roAssert(count == 0);
	while(freeBegin) {
		TaskProxy* next = freeBegin->nextFree;
		roAssert(!freeBegin->task);
		_allocator.deleteObj(freeBegin);
		freeBegin = next;
	}
}

TaskPool::TaskProxy* TaskPool::TaskList::alloc()
{
	++count;
	++idCounter;

	if(!freeBegin)
		freeBegin = _allocator.newObj<TaskProxy>().unref();

	TaskProxy* ret = freeBegin;
	freeBegin = ret->nextFree;
	ret->nextFree = NULL;

	ret->id = idCounter;
	roAssert(ret->task == NULL);

	return ret;
}

void TaskPool::TaskList::free(TaskProxy* id)
{
#if roDEBUG
	for(TaskProxy* p = freeBegin; p; p = p->nextFree)
		roAssert(p != id);
#endif

	*id = TaskProxy();
	id->nextFree = freeBegin;
	freeBegin = id;
	roAssert(count > 0);
	--count;
}

TaskPool::TaskPool()
	: _keepRun(true)
	, _openTasks(NULL)
	, _pendingTasksHead(NULL), _pendingTasksTail(NULL)
	, _pendingTaskCount(0)
	, _threadCount(0)
	, _workerWaitCount(0)
	, _threadHandles(NULL)
	, _mainThreadId(TaskPool::threadId())
	, _avgTaskPerSecond(0)
{
	_pendingTasksHead = _allocator.newObj<TaskProxy>().unref();
	_pendingTasksTail = _allocator.newObj<TaskProxy>().unref();

	_pendingTasksHead->nextPending = _pendingTasksTail;
	_pendingTasksTail->prevPending = _pendingTasksHead;
}

TaskPool::~TaskPool()
{
	ThreadId tId = threadId();
	ScopeLock lock(condVar);

	_keepRun = false;
	while(_openTasks)
		_wait(_openTasks, tId);

	condVar.broadcast();

	for(roSize i=0; i<_threadCount; ++i) {
		ScopeUnlock unlock(condVar);

#ifdef roUSE_PTHREAD
		pthread_t h = reinterpret_cast<pthread_t>(_threadHandles[i]);
		roVerify(::pthread_join(h, NULL) == 0);
		::pthread_detach(h);
#else
		HANDLE h = reinterpret_cast<HANDLE>(_threadHandles[i]);
		roVerify(::WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0);
		roVerify(::CloseHandle(h));
#endif
	}

	_allocator.free(_threadHandles);

	_allocator.deleteObj(_pendingTasksHead);
	_allocator.deleteObj(_pendingTasksTail);
}

void TaskPool::sleep(int ms)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

#ifdef roUSE_PTHREAD
	usleep(useconds_t(ms * 1000));
#else
	::Sleep(DWORD(ms));
#endif
}

void TaskPool::yield()
{
#ifdef roOS_WIN
	::SwitchToThread();
#elif defined roOS_UNIX
	sched_yield();
#else
	::Sleep(0);
#endif
}

void _run(TaskPool* pool)
{
	String s("WORKER THREAD");	// NOTE: This make sure every thread have it's own copy of the scope name
	CpuProfilerScope cpuProfilerScope(s.c_str());

	while(pool->keepRun()) {
		pool->doSomeTask(0);

		ScopeLock lock(pool->condVar);
		if(pool->_keepRun) {
			++pool->_workerWaitCount;
			pool->condVar.waitNoLock();
			--pool->_workerWaitCount;
		}
	}
}

#ifdef roUSE_PTHREAD
static void* _threadFunc(void* p) {
#else
static DWORD WINAPI _threadFunc(LPVOID p) {
#endif
	TaskPool* pool = reinterpret_cast<TaskPool*>(p);
	_run(pool);

	return 0;
}

void TaskPool::init(roSize threadCount)
{
	roAssert(!_threadHandles);
	_threadCount = threadCount;
	_threadHandles = _allocator.malloc(threadCount * sizeof(roSize));

	for(roSize i=0; i<_threadCount; ++i) {
#ifdef roUSE_PTHREAD
		roVerify(::pthread_create(reinterpret_cast<pthread_t*>(&_threadHandles[i]), NULL, &_threadFunc, this) == 0);
#else
		_threadHandles[i] = reinterpret_cast<roSize>(::CreateThread(NULL, 0, &_threadFunc, this, 0, NULL));
#endif
	}
}

TaskId TaskPool::beginAdd(Task* task, ThreadId affinity)
{
	ScopeLock lock(condVar);

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
	ScopeLock lock(condVar);

	TaskProxy* parentProxy = _findProxyById(parent);
	TaskProxy* childProxy = _findProxyById(child);

	if(!parentProxy || !childProxy)
		return;	// TODO: Gives warning?

	roAssert(parentProxy && !parentProxy->finalized && "Parameter 'parent' has already finalized");
	roAssert(childProxy && !childProxy->finalized && "Parameter 'child' has already finalized");
	roAssert(!childProxy->parent && "The given child task is already under");

	_retainTask(parentProxy);	// Paired with releaseTask() in _doTask()
	childProxy->parent = parentProxy;
}

void TaskPool::dependsOn(TaskId src, TaskId on)
{
	ScopeLock lock(condVar);

	TaskProxy* srcProxy = _findProxyById(src);
	TaskProxy* onProxy = _findProxyById(on);

	roAssert(srcProxy && !srcProxy->finalized && "Parameter 'src' has already finalized");

	srcProxy->dependency = onProxy;
	srcProxy->dependencyId = on;
}

void TaskPool::finishAdd(TaskId id)
{
	ScopeLock lock(condVar);

	if(TaskProxy* p = _findProxyById(id)) {
		roAssert(!p->finalized && "Please call finishAdd() only once");
		p->finalized = true;
		_releaseTask(p);
	}
}

TaskId TaskPool::addFinalized(Task* task, TaskId parent, TaskId dependency, ThreadId affinity)
{
	ScopeLock lock(condVar);

	TaskProxy* proxy = taskList.alloc();
	proxy->taskPool = this;
	proxy->task = task;
	proxy->affinity = affinity;
	proxy->finalized = true;

	if(parent != 0) {
		TaskProxy* parentProxy = _findProxyById(parent);
		roAssert(parentProxy && !parentProxy->finalized && "Parameter 'parent' has already finalized");
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

ThreadId TaskPool::threadId()
{
	ThreadId id;
#ifdef roUSE_PTHREAD
	id = ThreadId(::pthread_self());
#else
	id = ::GetCurrentThreadId();
#endif
	return id;
}

// NOTE: Recursive and re-entrant
void TaskPool::wait(TaskId id)
{
	ThreadId tId = threadId();
	ScopeLock lock(condVar);
	if(TaskProxy* p = _findProxyById(id))
		_wait(p, tId);
}

void TaskPool::waitAll()
{
	ThreadId tId = threadId();
	ScopeLock lock(condVar);

	while(_openTasks)
		_wait(_openTasks, tId);
}

#if DEBUG_PRINT
static int _debugWaitCount = 0;
static const int _debugMaxIndent = 10;
static const char _debugIndent[_debugMaxIndent+1] = "          ";
#endif

void TaskPool::_wait(TaskProxy* p, ThreadId tId)
{
	roAssert(condVar.isLocked());
	if(!p) return;

#if DEBUG_PRINT
	printf("%sBegin _wait(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
	++_debugWaitCount;
#endif

	TaskId id = p->id;

	// NOTE: No need to check for dependency, making the dependency chain faster
	// to clear up, at the expense of deeper callstack.
	if(_matchAffinity(p, tId)) {
		_doTask(p, tId);

		// Check if the task has been re-scheduled or having child tasks such that
		// the task is still not consider finished.
		if(p->id == id)
			goto DoOtherTasks;
	}
	else {
		// Do other tasks until the task in question has finished
		// by other threads or it's child got finished too.
	DoOtherTasks:
		// If p->id not equals to id, it means the task is really finished,
		// and the proxy may get reused with a new id.
		while(p->id == id) {
			TaskProxy* p2 = _pendingTasksHead->nextPending;

			// Search for a task having no dependency, to prevent dead lock
			while(p2 && (!_matchAffinity(p2, tId) || _hasOutstandingDependency(p2))) {
				p2 = p2->nextPending;
				continue;
			}

			if(p2 && p2 != _pendingTasksTail)
				_doTask(p2, tId);
			else {
				// If no more task can do, we wait
				++_workerWaitCount;
				condVar.waitNoLock();
				--_workerWaitCount;
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
	ScopeLock lock(condVar);
	return _findProxyById(id) == NULL;
}

void TaskPool::suspend(TaskId id)
{
	ScopeLock lock(condVar);
	if(TaskProxy* p = _findProxyById(id))
		p->suspensionCount++;
}

void TaskPool::resume(TaskId id)
{
	ScopeLock lock(condVar);
	if(TaskProxy* p = _findProxyById(id))
		if(p->suspensionCount > 0)
			p->suspensionCount--;
}

void TaskPool::doSomeTask(float timeout)
{
	ScopeLock lock(condVar);

	TaskProxy* p = _pendingTasksHead->nextPending;

	// Early exit
	if(p == _pendingTasksTail)
		return;

	CountDownTimer countDown(timeout);
	ThreadId tId = threadId();

	// A counter for preventing endless looping of the mixed p->nextPending/p->dependency chasing
	roSize loopCount = 0;
	while(p && p != _pendingTasksTail) {
		if(_matchAffinity(p, tId) && !_hasOutstandingDependency(p) && p->task)
		{
			_doTask(p, tId);

			// NOTE: Each time _doTask() is invoked, we start to search task
			// at the beginning, to avoid the problem of 'next' become invalid,
			// may be in-efficient, but easier to implement.
			p = _pendingTasksHead->nextPending;
			loopCount = 0;

			if(timeout > 0 && countDown.isExpired(_avgTaskPerSecond))
				return;
		}
		else {
			// Hunt for most depending job, to prevent job starvation.
			p = (p->dependency && (p->dependency->id == p->dependencyId)) ? p->dependency : p->nextPending;

			if(++loopCount > _pendingTaskCount) {
				if(_workerWaitCount > 0)
					condVar.broadcast();
				break;
			}
		}
	}
}

// NOTE: Recursive and re-entrant
void TaskPool::_doTask(TaskProxy* p, ThreadId tId)
{
	roAssert(condVar.isLocked());
	
	if(!p || !p->task) return;

#if DEBUG_PRINT
	printf("%sBegin _doTask(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif

	Task* task = p->task;
	p->task = NULL;

	roAssert(p->finalized);
	_removePendingTask(p);

	// NOTE: _wait() may trigger many things, therefore we need to _retainTask() here
	_retainTask(p);

	if(TaskProxy* dep = p->dependency) {
		if(dep->id == p->dependencyId) {
			_wait(dep, tId);
		}
	}

	task->_proxy = p;

	{	ScopeUnlock unlock(condVar);
		task->run(this);
		task = NULL;	// The task may be deleted, never use the pointer up to this point
	}

#if DEBUG_PRINT
	printf("%sEnd _doTask(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif

	if(TaskProxy* parent = p->parent)
		_releaseTask(parent);

	_releaseTask(p);

	if(_workerWaitCount > 0) {
		// NOTE: After a task has finished, wake up all workers so they have a chance
		// to notice about the dependency update
		condVar.broadcast();
	}
}

TaskPool::TaskProxy* TaskPool::_findProxyById(TaskId id)
{
	roAssert(condVar.isLocked());

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
	roAssert(condVar.isLocked());
	roAssert(p->finalized);
	p->openChildCount--;
	if(p->openChildCount == 0 && !p->task)
		_removeOpenTask(p);
}

void TaskPool::_removeOpenTask(TaskProxy* p)
{
	roAssert(condVar.isLocked());
	roAssert(!p->task);
	if(p->prevOpen) p->prevOpen->nextOpen = p->nextOpen;
	if(p->nextOpen) p->nextOpen->prevOpen = p->prevOpen;
	if(p == _openTasks) _openTasks = p->nextOpen;
	taskList.free(p);
}

void TaskPool::_addPendingTask(TaskProxy* p)
{
	roAssert(condVar.isLocked());

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

	++_pendingTaskCount;

	if(_workerWaitCount > 0) {
		// NOTE: We need to do broadcast rather than signal, because we don't know
		// which worker to wake up such that it match the affinity define in the task
		condVar.broadcast();
	}
}

void TaskPool::_removePendingTask(TaskProxy* p)
{
	roAssert(condVar.isLocked());
	if(p->prevPending) p->prevPending->nextPending = p->nextPending;
	if(p->nextPending) p->nextPending->prevPending = p->prevPending;
	p->prevPending = p->nextPending = NULL;
	roAssert(_pendingTaskCount > 0);
	--_pendingTaskCount;
}

bool TaskPool::_matchAffinity(TaskProxy* p, ThreadId tId)
{
	if(p->suspensionCount > 0 || !p->finalized)
		return false;

	if(p->affinity < 0)	// Only this thread cannot run
		return _threadCount == 0 ? true : ~p->affinity != tId;

	return	p->affinity == 0 ||		// Any thread can run
			p->affinity == tId;		// Only this thread can run
}

bool TaskPool::_hasOutstandingDependency(TaskPool::TaskProxy* p)
{
	return	p->dependency &&
		p->dependency->id == p->dependencyId;	// Check if task finished
}

namespace {
	
class CallbackTask : public Task
{
public:
	override void run(TaskPool* taskPool)
	{
		callback(taskPool, userData);
		_allocator.deleteObj(this);
	}
	void* userData;
	TaskPool::Callback callback;
};
	
}	// namespace

// NOTE: This function is purely build on top of other public interface of TaskPool ^.^
void TaskPool::addCallback(TaskId id, Callback callback, void* userData, ThreadId affinity)
{
	CallbackTask* t = _allocator.newObj<CallbackTask>().unref();
	t->callback = callback;
	t->userData = userData;
	addFinalized(t, 0, id, affinity);
}

void Task::reSchedule(bool suspend)
{
	TaskPool::TaskProxy* p = reinterpret_cast<TaskPool::TaskProxy*>(this->_proxy);

	ScopeLock lock(p->taskPool->condVar);

	p->task = this;
	p->taskPool->_addPendingTask(p);
	p->suspensionCount += suspend;

#if DEBUG_PRINT
	printf("%sreSchedule(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif
}

void Task::reSchedule(bool suspend, ThreadId affinity)
{
	TaskPool::TaskProxy* p = reinterpret_cast<TaskPool::TaskProxy*>(this->_proxy);

	ScopeLock lock(p->taskPool->condVar);

	p->task = this;
	p->affinity = affinity;
	p->taskPool->_addPendingTask(p);
	p->suspensionCount += suspend;

#if DEBUG_PRINT
	printf("%sreSchedule(%d)\n", _debugIndent + _debugMaxIndent - _debugWaitCount, p->id);
#endif
}

}	// namespace ro
