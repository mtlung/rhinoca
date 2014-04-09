#include "pch.h"
#include "roCoroutine.h"
#include "roCoroutine.inc"
#include "roLog.h"

#ifdef _MSC_VER
#	pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

namespace ro {

__declspec(thread) static Coroutine* _currentCoroutine = NULL;

static void coroutineFunc(void* userData)
{
	Coroutine* coroutine = reinterpret_cast<Coroutine*>(userData);
	roAssert(coroutine);
	roAssert(coroutine->scheduler);

	CoroutineScheduler* scheduler = coroutine->scheduler;
	coro_context* contextToTransfer = &coroutine->_context;

	_currentCoroutine = coroutine;
	coroutine->_isInRun = true;
	coroutine->_isActive = true;
	coroutine->_runningThreadId = TaskPool::threadId();

	coroutine->run();

	_currentCoroutine = NULL;
	if(scheduler->_destroiedCoroutine == coroutine)
		contextToTransfer = &scheduler->_contextToDestroy;
	else {
		coroutine->_isInRun = false;
		coroutine->_isActive = false;
		coroutine->_runningThreadId = 0;
	}

	// Switch back to CoroutineScheduler
	coro_transfer(contextToTransfer, &scheduler->context);
}

__declspec(thread) CoroutineScheduler* tlsInstance = NULL;

CoroutineScheduler::CoroutineScheduler()
	: current(NULL)
	, _destroiedCoroutine(NULL)
{}

static Coroutine* createSleepManager();

void CoroutineScheduler::init(roSize stackSize)
{
	_keepRun = true;
	_stack.resizeNoInit(stackSize);
	coro_create(&context, NULL, NULL, NULL, 0);

	add(*createSleepManager());
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

			roByte* stackPtr = NULL;
			roSize stackSize = 0;

			if(co->_ownStack.isEmpty()) {
				stackPtr = _stack.bytePtr();
				stackSize = _stack.sizeInByte();
			}
			else {
				stackPtr = co->_ownStack.bytePtr();
				stackSize = _stack.sizeInByte();co->_ownStack.sizeInByte();
			}

			// Check for alignment
			roSize alignemt = sizeof(void*);
			roByte* alignedStackPtr = (roByte*)(((roPtrInt(stackPtr) - 1) / alignemt) * alignemt + alignemt);
			stackSize -= (alignedStackPtr - stackPtr);
			coro_create(&co->_context, coroutineFunc, (void*)co, alignedStackPtr, stackSize);
		}

		// Copy stack from backup to running stack
		roSize stackUsed = co->_backupStack.sizeInByte();
		if(stackUsed) {
			roMemcpy(&_stack.back() - stackUsed, co->_backupStack.bytePtr(), stackUsed);
			co->_backupStack.clear();
		}

		_currentCoroutine = co;
		co->scheduler = this;
		co->_isActive = true;
		co->removeThis();
		coro_transfer(&context, &co->_context);

		if(_destroiedCoroutine == co) {
			_destroiedCoroutine = NULL;
			coro_destroy(&_contextToDestroy);
		}

		_currentCoroutine = NULL;
		current = NULL;
	}

	tlsInstance = NULL;
}

void CoroutineScheduler::requestStop()
{
	_keepRun = false;
}

bool CoroutineScheduler::keepRun() const
{
	return _keepRun;
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
	if(_isInRun && _isActive) {	// Calling "delete this" inside run()
		scheduler->_destroiedCoroutine = this;
		scheduler->_contextToDestroy = _context;

		// Prevent _ownStack to be destroyed right now, since we are still running on this stack memory!
		roSwap(_ownStack, scheduler->_stackToDestroy);
	}
	else {
		if(_isInRun) {
			roAssert(!_isInRun);
			roLog("error", "Please let coroutine '%s' finish, before it is destroyed\n", debugName.c_str());
		}

		coro_destroy(&_context);
	}
}

roStatus Coroutine::initStack(roSize stackSize)
{
	if(_isInRun || _isActive)
		return roStatus::assertion;

	_backupStack.clear();
	_backupStack.condense();
	return _ownStack.resizeNoInit(stackSize);
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
	if(_ownStack.isEmpty()) {
		// See how much stack we are using
		roByte* currentStack = (roByte*)_getCurrentStackPtr();
		roSize stackUsed = (&scheduler->_stack.back()) - currentStack;

		// Backup the current stack
		_backupStack.assign((&scheduler->_stack.back()) - stackUsed, stackUsed);
	}
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

Coroutine* Coroutine::current()
{
	return _currentCoroutine;
}

}	// namespace ro

#if roCOMPILER_VC && roOS_WIN64
// Compiled from asm file
#elif roCOMPILER_VC && roOS_WIN32
__declspec(naked) int roSetJmp(jmp_buf)
{__asm {
	mov eax,DWORD PTR [esp+4]
	mov edx,DWORD PTR [esp+0]
	mov DWORD PTR [eax+0],ebp
	mov DWORD PTR [eax+4],ebx
	mov DWORD PTR [eax+8],edi
	mov DWORD PTR [eax+12],esi
	mov DWORD PTR [eax+16],esp
	mov DWORD PTR [eax+20],edx
	xor eax,eax
	ret
}}

__declspec(naked) void roLongJmp(jmp_buf, int)
{__asm {
	mov edx,DWORD PTR [esp+4]
	mov eax,DWORD PTR [esp+8]
	mov ebp,DWORD PTR [edx+0]
	mov ebx,DWORD PTR [edx+4]
	mov edi,DWORD PTR [edx+8]
	mov esi,DWORD PTR [edx+12]
	mov esp,DWORD PTR [edx+16]
	mov ecx,DWORD PTR [edx+20]
	test eax,eax
	jne a
	inc eax
a: mov DWORD PTR [esp],ecx
	ret
}}

#else
int roSetJmp(jmp_buf buf)
{
	return setJmp(buf);
}

void roLongJmp(jmp_buf buf, int val)
{
	longjmp(buf, val);
}
#endif

#include "roStopWatch.h"

namespace ro {

struct CoSleepManager;

__declspec(thread) static CoSleepManager* _currentCoSleepManager = NULL;

struct CoSleepManager : public Coroutine
{
	struct Entry
	{
		float timeToWake;
		Coroutine* coro;
		static bool less(const Entry& lhs, const Entry& rhs)
		{
			return lhs.timeToWake < rhs.timeToWake;
		}
	};

	CoSleepManager()
	{
		roAssert(!_currentCoSleepManager);
		_currentCoSleepManager = this;
	}

	~CoSleepManager()
	{
		roAssert(_currentCoSleepManager == this);
		_currentCoSleepManager = NULL;
	}

	void add(Coroutine& co, float seconds)
	{
		roAssert(_currentCoSleepManager == this);
		const float timeToWake = stopWatch.getFloat() + seconds;

		Entry entry = { timeToWake, &co };
		sortedEntries.insertSorted(entry, Entry::less);
	}

	virtual void run() override
	{
		stopWatch.reset();

		while(scheduler->keepRun() || !sortedEntries.isEmpty()) {
			const float currentTime = stopWatch.getFloat();
			while(!sortedEntries.isEmpty()) {
				const Entry& entry = sortedEntries.front();
				if(entry.timeToWake > currentTime)
					break;

				entry.coro->resume();
				sortedEntries.remove(0);
			}

			yield();
			roAssert(_currentCoSleepManager == this);
		}

		delete this;
	}

	StopWatch stopWatch;
	Array<Entry> sortedEntries;
};

Coroutine* createSleepManager()
{
	return new CoSleepManager;
}

void coSleep(float seconds)
{
	Coroutine* coroutine = Coroutine::current();
	CoSleepManager* sleepMgr = _currentCoSleepManager;
	roAssert(coroutine && sleepMgr);

	if(seconds == 0) {
		coroutine->yield();
		return;
	}

	sleepMgr->add(*coroutine, seconds);
	coroutine->suspend();
}

}	// namespace ro
