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

	virtual void run() = 0;

	void yield();	// Temporary give up the processing ownership
	void suspend();	// Give up processing ownership, until someone call resume()
	void resume();	// Move the Coroutine back on track, after it's has been suspend()

	void switchToCoroutine(Coroutine& coroutine);
	void switchToThread(ThreadId threadId);	// Similar to yield(), but resume on the specific thread
	void resumeOnThread(ThreadId threadId);

	bool				_isInRun;	// Is inside the run() function
	bool				_isActive;
	ThreadId			_runningThreadId;
	ByteArray			_backupStack;
	coro_context		_context;
	ConstString			debugName;
	CoroutineScheduler*	scheduler;
};	// Coroutine

struct CoroutineScheduler
{
	CoroutineScheduler();

	void init(roSize statckSize = 1024 * 1024);
	void add(Coroutine& coroutine);
	void addSuspended(Coroutine& coroutine);
	void update();

	// Yield the current running co-routine and return it's pointer, so you can resume it later on.
	// Coroutine should not be destroied before run() finish, so it's safe to return Coroutine*
	static Coroutine* currentCoroutine();
	static CoroutineScheduler* perThreadScheduler();

	coro_context context;
	Coroutine* current;

	LinkList<Coroutine> _resumeList;
	ByteArray _stack;
};	// CoroutineScheduler

}	// namespace ro

#endif	// __roCoroutine_h__
