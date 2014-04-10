#ifndef __roCoroutine_h__
#define __roCoroutine_h__

#include "roArray.h"
#include "roLinkList.h"
#include "roString.h"
#include "roTaskPool.h"
#include "roCoroutine.inl"

namespace ro {

struct CoroutineScheduler;

// Stackful - can yield in subsequently nest child call.
// Make sure run() has finished before Coroutine is deleted.
// Web server written in coroutine:
// http://matt.might.net/articles/pipelined-nonblocking-extensible-web-server-with-coroutines/
struct Coroutine : public ListNode<Coroutine>
{
	Coroutine();
	~Coroutine();

	// Calling initStack is optional; if no private stack assigned,
	// the shared stack in CoroutineScheduler will be used and swap in/our with _backupStack.
	roStatus initStack(roSize stackSize = 1024 * 1024);

	virtual void run() = 0;

	void yield();	// Temporary give up the processing ownership
	void suspend();	// Give up processing ownership, until someone call resume()
	void resume();	// Move the Coroutine back on track, after it's has been suspend()

	void switchToCoroutine(Coroutine& coroutine);
	void switchToThread(ThreadId threadId);	// Similar to yield(), but resume on the specific thread
	void resumeOnThread(ThreadId threadId);

	// Get the current thread's active co-routine, if any
	static Coroutine* current();

	bool				_isInRun;		// Is inside the run() function
	bool				_isActive;
	bool				_isBackground;	// Background coroutine is used to implement blocking service like sleep and network IO etc
	ThreadId			_runningThreadId;
	ByteArray			_ownStack;
	ByteArray			_backupStack;
	coro_context		_context;
	ConstString			debugName;
	CoroutineScheduler*	scheduler;
};	// Coroutine

struct CoroutineScheduler
{
	CoroutineScheduler();
	~CoroutineScheduler();

	void init(roSize statckSize = 1024 * 1024);
	void add(Coroutine& coroutine);
	void addSuspended(Coroutine& coroutine);
	void update(unsigned timeSliceMilliSeconds=0);
	void requestStop();
	void stop();
	bool keepRun() const;

	// Yield the current running co-routine and return it's pointer, so you can resume it later on.
	// Coroutine should not be destroied before run() finish, so it's safe to return Coroutine*
	static Coroutine* currentCoroutine();
	static CoroutineScheduler* perThreadScheduler();

	coro_context		context;
	Coroutine*			current;

	bool				_keepRun;
	roSize				_backgroundCoroCount;
	LinkList<Coroutine> _resumeList;
	LinkList<Coroutine> _suspendedList;
	ByteArray			_stack;					// Multiple coroutine run on the same stack. Stack will be copied when switching coroutines
	void*				_destroiedCoroutine;	// Coroutine that have been destroied in run() function
	coro_context		_contextToDestroy;		// We have to delay the destruction of context, until we switch back coro to the scheduler
	ByteArray			_stackToDestroy;
};	// CoroutineScheduler

void coSleep(float seconds);

}	// namespace ro

#endif	// __roCoroutine_h__
