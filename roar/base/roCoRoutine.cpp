#include "pch.h"
#include "roCoroutine.h"
#include "roCoroutine.inc"
#include "roLog.h"

#ifdef _MSC_VER
#	pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

namespace ro {

static void coroutineFunc(void* userData)
{
	Coroutine* coroutine = reinterpret_cast<Coroutine*>(userData);
	roAssert(coroutine);
	roAssert(coroutine->scheduler);

	coroutine->_isInRun = true;
	coroutine->_isActive = true;
	coroutine->_runningThreadId = TaskPool::threadId();

	coroutine->run();

	coroutine->_isInRun = false;
	coroutine->_isActive = false;
	coroutine->_runningThreadId = 0;

	// TODO: Detect if coroutine had been deleted

	// Switch back to CoroutineScheduler
	coro_transfer(&coroutine->_context, &coroutine->scheduler->context);
}

__declspec(thread) CoroutineScheduler* tlsInstance = NULL;

CoroutineScheduler::CoroutineScheduler()
	: current(NULL)
{}

void CoroutineScheduler::init(roSize stackSize)
{
	_stack.resizeNoInit(stackSize);
	coro_create(&context, NULL, NULL, NULL, 0);
}

void CoroutineScheduler::add(Coroutine& coroutine)
{
	_resumeList.pushBack(coroutine);
}

void CoroutineScheduler::addSuspended(Coroutine& coroutine)
{
	coroutine.scheduler = this;
}

void CoroutineScheduler::update()
{
	roAssert(!_stack.isEmpty());

	tlsInstance = this;

	while(!_resumeList.isEmpty()) {
		Coroutine* co = &_resumeList.front();
		current = co;

		if(!co->scheduler) {
			co->_backupStack.clear();
			coro_create(&co->_context, coroutineFunc, (void*)co, _stack.bytePtr(), _stack.sizeInByte());
		}

		// Copy stack from backup to running stack
		roSize stackUsed = co->_backupStack.sizeInByte();
		if(stackUsed) {
			roMemcpy(&_stack.back() - stackUsed, co->_backupStack.bytePtr(), stackUsed);
			co->_backupStack.clear();
		}

		co->scheduler = this;
		co->_isActive = true;
		co->removeThis();
		coro_transfer(&context, &co->_context);
		current = NULL;
	}

	tlsInstance = NULL;
}

Coroutine* CoroutineScheduler::currentCoroutine()
{
	if(!tlsInstance)
		return NULL;

	return tlsInstance->current;
}

CoroutineScheduler* CoroutineScheduler::perThreadScheduler()
{
	return tlsInstance;
}

Coroutine::Coroutine()
	: scheduler(NULL)
	, _isInRun(false)
	, _isActive(false)
	, _runningThreadId(0)
{
	coro_create(&_context, NULL, NULL, NULL, 0);
}

Coroutine::~Coroutine()
{
	if(_isInRun)
		roLog("error", "Please let coroutine '%s' finish, before it is destroyed\n", debugName.c_str());
	roAssert(!_isInRun);
	coro_destroy(&_context);
}

void Coroutine::yield()
{
	roAssert(_isActive);

	scheduler->_resumeList.pushBack(*this);
	suspend();
}

#if !CORO_FIBER
#if roCOMPILER_VC
#pragma warning(disable : 4172)	// Returning address of local variable or temporary
__declspec(noinline)
#endif
static void* _getCurrentStackPtr()
{
	char dummy;
	return (void*)&dummy;
}
#endif

void Coroutine::suspend()
{
	roAssert(_isInRun);
	roAssert(_isActive);
	roAssert(scheduler);
	roAssert(_runningThreadId == TaskPool::threadId());

	_isActive = false;

#if !CORO_FIBER
	// See how much stack we are using
	roByte* currentStack = (roByte*)_getCurrentStackPtr();
	roSize stackUsed = (&scheduler->_stack.back()) - currentStack;

	// Backup the current stack
	_backupStack.assign((&scheduler->_stack.back()) - stackUsed, stackUsed);
#endif

	coro_transfer(&_context, &scheduler->context);
}

void Coroutine::resume()
{
	roAssert(_isInRun);
	roAssert(scheduler);

	if(_isActive)
		return;

	// Put this Coroutine back to schedule
	scheduler->_resumeList.pushBack(*this);
}

void Coroutine::switchToCoroutine(Coroutine& coroutine)
{
	roAssert(_isInRun);
	roAssert(_isActive);
	if(coroutine._isActive)	// Handle the case switching to self
		return;

	coroutine.resume();
	suspend();
}

void Coroutine::switchToThread(ThreadId threadId)
{
	roAssert(false && "To be implement");
}

void Coroutine::resumeOnThread(ThreadId threadId)
{
	roAssert(false && "To be implement");
}

}	// namespace ro
