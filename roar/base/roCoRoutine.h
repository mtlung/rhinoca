#ifndef __roCoroutine_h__
#define __roCoroutine_h__

#include "roArray.h"
#include "roLinkList.h"
#include "roStopWatch.h"
#include "roString.h"
#include "roTaskPool.h"
#include "roCoroutine.inl"
#include <functional>

namespace ro {

struct CoroutineScheduler;

// Stackful - can yield in subsequently nest child call.
// Make sure run() has finished before Coroutine is deleted.
// Web server written in coroutine:
// http://matt.might.net/articles/pipelined-nonblocking-extensible-web-server-with-coroutines/
// Coroutine intro
// http://blog.panicsoftware.com/coroutines-introduction/
// Comparable library
// https://swtch.com/libtask/
// https://github.com/google/marl
// http://state-threads.sourceforge.net/
// Go-routine are going more preemptive
// https://github.com/golang/proposal/blob/master/design/24543-non-cooperative-preemption.md
struct Coroutine : public ListNode<Coroutine>
{
	Coroutine();
	~Coroutine();

	// Set the max stack size allowed
	// Virtual memory will be allocated on demand
	roStatus initStack(roSize maxStackSize = defualtMaxStackSize);

	virtual void run() = 0;

	void	yield				();	// Temporary give up the processing ownership
	void	yieldFrame			();	// Temporary give up the processing ownership and won't get resumed until next frame
	void*	suspend				();	// Give up processing ownership, until someone call resume().  TODO: Using void* as return value is not type safe
	void	resume				(void* retValueForSuspend = NULL);	// Move the Coroutine back on track, after it's has been suspend()

	void*	suspendWithId		(void* id);
	void	resumeWithId		(void* id, void* retValueForSuspend = NULL);

	void	switchToCoroutine	(Coroutine& coroutine);
	void	switchToThread		(ThreadId threadId);	// Similar to yield(), but resume on the specific thread
	void	resumeOnThread		(ThreadId threadId);

	bool	isSuspended			() const;

	// Get the current thread's active co-routine, if any
	static Coroutine* current();

	// Reclaim committed memory base on the current stack position
	static void shrinkStack();

	static const roSize defualtMaxStackSize = roMB(4);

// Private
	bool				_isInRun;		// Is inside the run() function
	bool				_isActive;
	void*				_retValueForSuspend;
	void*				_suspenderId;	// Only the one who can provide the same id can resume
	void*				_profilerNode;	// Act as a coroutine specific variable for CpuProfiler
	ThreadId			_runningThreadId;

	coro_stack			_stack;
	coro_context		_context;
	ConstString			debugName;
	CoroutineScheduler*	scheduler;

	static const bool	_isStackGrowthReverse = true;
	static const roSize	_stackPadding = sizeof(void*) * 2;	// Keep a padding at the "bottom" of the stack, and tweak it so stack walker won't got confused
};	// Coroutine

struct BgCoroutine : public Coroutine
{
	_ListNode bgNode;
};

struct CoroutineScheduler
{
	CoroutineScheduler();
	~CoroutineScheduler();

	roStatus	init				();
	void		add					(Coroutine& coroutine);
	roStatus	add					(const std::function<void()>& func, const char* debugName="unamed std::function", size_t stackSize = Coroutine::defualtMaxStackSize);
	void		addFront			(Coroutine& coroutine);
	roStatus	addFront			(const std::function<void()>& func, const char* debugName="unamed std::function", size_t stackSize = Coroutine::defualtMaxStackSize);
	void		addSuspended		(Coroutine& coroutine);
	roStatus	update				(unsigned timeSliceMilliSeconds=0, roUint64* nextUpdateTime=nullptr);
	void		runTillAllFinish	(float maxFps=60.f);
	void		requestStop			();
	void		stop				();
	bool		keepRun				() const;
	bool		bgKeepRun			() const;

	roSize		taskCount			() const;	// Exclude background task
	roSize		activetaskCount		() const;	//
	roSize		suspendedtaskCount	() const;	//

	roUint64	currentTimeInTicks	() const;	// For use in conjunction with update(nextUpdateTime)

	// Yield the current running co-routine and return it's pointer, so you can resume it later on.
	// Coroutine should not be destroied before run() finish, so it's safe to return Coroutine*
	static Coroutine* currentCoroutine();
	static CoroutineScheduler* perThreadScheduler();

	coro_context		context;
	Coroutine*			current;

	struct BackgroundNode : public ListNode<BackgroundNode> {};

	bool				_keepRun;
	bool				_bgKeepRun;
	bool				_inUpdate;
	LinkList<Coroutine> _resumeList;
	LinkList<Coroutine> _resumeNextFrameList;
	LinkList<Coroutine> _suspendedList;
	LinkList<_ListNode>	_backgroundList;		// For those coroutine that provide very basic service (eg. sleep and IO) should register themselves in this list
	void*				_destroiedCoroutine;	// Coroutine that have been destroied in run() function
	coro_context		_contextToDestroy;		// We have to delay the destruction of context, until we switch back coro to the scheduler
	coro_stack			_stackToDestroy;
	BgCoroutine*		_sleepManager;
};	// CoroutineScheduler

roStatus	coRun(const std::function<void()>& func, const char* debugName = "unamed std::function", size_t stackSize = Coroutine::defualtMaxStackSize);
bool		coSleep(float seconds);			// Returns false if unblocked by coWakeup()
void		coWakeup(Coroutine* coroutine);	// Wake up the coroutine suspended by coSleep(). Passing a NULL coroutine will be ignored
void		coYield();
void		coYieldFrame();

roStatus	ioEventInit();
roStatus	ioEventRegister(void* fdCtx);
roStatus	ioEventRead(const void* ioCtx);
roStatus	ioEventWrite(const void* ioCtx);
roStatus	ioEventCancel(const void* ioCtx);
void		ioEventDispatch(const bool& keepRun, const float& timeAllowed);	// Will suspend current coroutine, you need to remember it using Coroutine::current(); and resume it later on
roStatus	ioEventShutdown();

}	// namespace ro

#endif	// __roCoroutine_h__
