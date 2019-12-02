#include "pch.h"
#include "roCoroutine.h"
#include "roCoroutine.inc"
#include "roCpuProfiler.h"
#include "roAtomic.h"
#include "roLog.h"
#include "roStackWalker.h"
#include "roUtility.h"

#ifdef _MSC_VER
#	pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

namespace ro {

//////////////////////////////////////////////////////////////////////////
// CoSleepManager

struct CoSleepManager;

thread_local CoSleepManager* _currentCoSleepManager = NULL;

struct CoSleepManager : public BgCoroutine
{
	struct Entry
	{
		roUint64 timeToWake;
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
		debugName = "CoSleepManager";
	}

	~CoSleepManager()
	{
		roAssert(_currentCoSleepManager == this);
		_currentCoSleepManager = NULL;
	}

	void add(Coroutine& co, float seconds)
	{
		roAssert(_currentCoSleepManager == this);
		const roUint64 timeToWake = stopWatch.getTick() + secondsToTicks(seconds);

		Entry entry = { timeToWake, &co };
		sortedEntries.insertSorted(entry, Entry::less);

		resumeWithId(this);
	}

	virtual void run() override
	{
		scheduler->_backgroundList.pushBack(bgNode);

		stopWatch.reset();

		while(scheduler->bgKeepRun() || !sortedEntries.isEmpty())
		{
			const roUint64 currentTime = stopWatch.getTick();
			while(!sortedEntries.isEmpty()) {
				const Entry& entry = sortedEntries.front();
				if(entry.timeToWake > currentTime)
					break;

				entry.coro->resumeWithId(this, NULL);
				sortedEntries.removeAt(0);
			}

			if(sortedEntries.isEmpty())
				suspendWithId(this);
			else
				yield();

			roAssert(_currentCoSleepManager == this);
		}

		delete this;
	}

	bool hasTaskToWakeNow() const
	{
		if(sortedEntries.isEmpty())
			return false;

		return sortedEntries.front().timeToWake < stopWatch.getTick();
	}

	roUint64 nextResumeTime() const {
		if(sortedEntries.isEmpty())
			return TypeOf<roUint64>::valueMax();
		else
			return sortedEntries.front().timeToWake;
	}

	StopWatch	stopWatch;
	Array<Entry> sortedEntries;
};	// CoSleepManager

BgCoroutine* createSleepManager()
{
	CoSleepManager* mgr = new CoSleepManager;
	mgr->initStack(1024 * 32);
	return mgr;
}

roStatus coRun(const std::function<void()>& func, const char* debugName, size_t stackSize)
{
	CoroutineScheduler* scheduler = CoroutineScheduler::perThreadScheduler();
	if(!scheduler)
		return roStatus::pointer_is_null;

	return scheduler->add(func, debugName, stackSize);
}

bool coSleep(float seconds)
{
	Coroutine* coroutine = Coroutine::current();
	CoSleepManager* sleepMgr = _currentCoSleepManager;

	if(coroutine && (!sleepMgr || seconds == 0)) {
		coroutine->yield();
		return true;
	}

	if(coroutine && sleepMgr) {
		sleepMgr->add(*coroutine, seconds);
		return coroutine->suspendWithId(sleepMgr) == NULL;
	}

	return true;
}

void coWakeup(Coroutine* coroutine)
{
	if (!coroutine)
		return;
	CoSleepManager* sleepMgr = _currentCoSleepManager;
	if (coroutine->_suspenderId != (void*)sleepMgr)
		return;

	bool(*equal)(const CoSleepManager::Entry&, const CoSleepManager::Entry&) = [](auto& a, auto& b) -> bool {
		return a.coro == b.coro;
	};
	sleepMgr->sortedEntries.removeByKey(CoSleepManager::Entry{ 0, coroutine }, equal);
	coroutine->resumeWithId(sleepMgr, (void*)1);
}

void coYield()
{
	Coroutine::current()->yield();
}

void coYieldFrame()
{
	Coroutine::current()->yieldFrame();
}

//////////////////////////////////////////////////////////////////////////
// CoroutineScheduler

thread_local Coroutine* _currentCoroutine = NULL;

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
		coroutine->removeThis();
	}

	// Switch back to CoroutineScheduler
	coro_transfer(contextToTransfer, &scheduler->context);
}

thread_local CoroutineScheduler* tlsInstance = NULL;

CoroutineScheduler::CoroutineScheduler()
	: current(NULL)
	, _keepRun(false)
	, _bgKeepRun(false)
	, _inUpdate(false)
	, _destroiedCoroutine(NULL)
	, _sleepManager(NULL)
{}

CoroutineScheduler::~CoroutineScheduler()
{
	roAssert(!_keepRun && !_bgKeepRun && !_inUpdate);
}

thread_local AtomicInteger _CoroutineSchedulerInitCount = 0;

roStatus CoroutineScheduler::init(roSize stackSize)
{
	if(++_CoroutineSchedulerInitCount > 1)
		return roStatus::already_initialized;

	_keepRun = true;
	_bgKeepRun = true;
	_stack.resizeNoInit(stackSize);
	coro_create(&context, NULL, NULL, NULL, 0);

	add(*createSleepManager());

	return ioEventInit();
}

void CoroutineScheduler::add(Coroutine& coroutine)
{
	_resumeList.pushBack(coroutine);
}

void CoroutineScheduler::addFront(Coroutine& coroutine)
{
	_resumeList.pushFront(coroutine);
}

static roStatus coroutineSchedulerAdd(CoroutineScheduler& self, const std::function<void()>& func, const char* debugName, size_t stackSize, bool front)
{
	struct FunctorCoroutine : public Coroutine, private NonCopyable
	{
		virtual void run() override { f(); delete this; }
		std::function<void()> f;
	};

	FunctorCoroutine* c = new FunctorCoroutine;
	roStatus st = c->initStack(stackSize); if(!st) return st;
	c->f = func;
	c->debugName = debugName;
	front ? self.addFront(*c) : self.add(*c);

	return st;
}

roStatus CoroutineScheduler::add(const std::function<void()>& func, const char* debugName, size_t stackSize)
{
	return coroutineSchedulerAdd(*this, func, debugName, stackSize, false);
}

roStatus CoroutineScheduler::addFront(const std::function<void()>& func, const char* debugName, size_t stackSize)
{
	return coroutineSchedulerAdd(*this, func, debugName, stackSize, true);
}

void CoroutineScheduler::addSuspended(Coroutine& coroutine)
{
	coroutine.scheduler = this;
}

roStatus CoroutineScheduler::update(unsigned timeSliceMilliSeconds, roUint64* nextUpdateTime)
{
	if(_stack.isEmpty())
		return roStatus::not_initialized;

	roAssert(!_stack.isEmpty());
	roAssert(!_inUpdate && "this is a non-reentrant function");

	_inUpdate = true;
	tlsInstance = this;
	float timerHit = 0;
	CountDownTimer timer(timeSliceMilliSeconds / 1000.0f);

	// Move all _resumeNextFrameList to _resumeList
	while (!_resumeNextFrameList.isEmpty())
		_resumeList.moveBack(_resumeNextFrameList.front());

	while(!_resumeList.isEmpty() && (timeSliceMilliSeconds == 0 || !timer.isExpired(timerHit)))
	{
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
				stackSize = co->_ownStack.sizeInByte();
			}

			stackSize = stackSize > Coroutine::_stackPadding ? stackSize - Coroutine::_stackPadding : 0;

			// Check for alignment
			const roSize alignment = 2 * sizeof(void*);
			roByte* alignedStackPtr = NULL;
			if(Coroutine::_isStackGrowthReverse) {
				roByte* p = stackPtr + stackSize;
				alignedStackPtr = (roByte*)(((roPtrInt(p) + alignment) / alignment) * alignment - alignment);
				stackSize -= (p - alignedStackPtr);
				alignedStackPtr -= stackSize;
				roAssert(alignedStackPtr == stackPtr);
			}
			else {
				alignedStackPtr = (roByte*)(((roPtrInt(stackPtr) - alignment) / alignment) * alignment + alignment);
				stackSize -= (alignedStackPtr - stackPtr);
			}

#if roCPU_x86
			// Fake the frame in our stack so the callstack looks calling from CoroutineScheduler::update
			if(Coroutine::_isStackGrowthReverse) {
				StackWalker stackWalker;
				stackWalker.init();
				void* frame[1];
				stackWalker.stackWalk(frame, roCountof(frame), NULL);
				void** stackBottomFrame = (void**)(alignedStackPtr + stackSize);
				--stackBottomFrame;
				stackBottomFrame[1] = frame[0];
			}
#else
			// To be implement
#endif

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

		// If the only task left is the sleep manager
		if(_resumeList.size() == 1) {
			CoSleepManager* sleepMgr = dynamic_cast<CoSleepManager*>(&_resumeList.front());
			if(sleepMgr && !sleepMgr->hasTaskToWakeNow())
				break;
		}
	}

	if(nextUpdateTime && _currentCoSleepManager)
		*nextUpdateTime = _currentCoSleepManager->nextResumeTime();

	tlsInstance = NULL;

	// Wake up all suspended background coroutine
	if(!keepRun() && _resumeList.isEmpty()) {
		for(_ListNode* i = _backgroundList.begin(); i != _backgroundList.end();) {
			_ListNode* next = i->_next;
			BgCoroutine* bg = roContainerof(BgCoroutine, bgNode, i);
			bg->resume();
			i = next;
		}
	}

	_inUpdate = false;

	return roStatus::ok;
}

void CoroutineScheduler::requestStop()
{
	_keepRun = false;
}

void CoroutineScheduler::runTillAllFinish(float maxFps)
{
	FrameRateRegulator regulator;
	regulator.setTargetFraemRate(maxFps);

	bool signal = false;
	bool keepRun = true;
	float elapsed = 0, timeRemain = 0;
	Coroutine* eventCoroutine = NULL;

	this->addFront([&]() {	// Use addFront to make sure ioEventDispatch run first
		eventCoroutine = currentCoroutine();
		ioEventDispatch(keepRun, timeRemain);
		signal = true;
	}, "IO Event dispatch loop", roKB(16));

	while(taskCount() > 0) {
		regulator.beginTask();
		roUint64 nextWakeUpTime;
		update(unsigned(1000.0f * regulator.targetFrameTime), &nextWakeUpTime);
		regulator.endTask(elapsed, timeRemain);

		float remainToWake = ticksToSeconds(nextWakeUpTime) - ticksToSeconds(currentTimeInTicks());
		timeRemain = roMinOf2(timeRemain, remainToWake);

		if(taskCount() == _backgroundList.size() + 1)	// +1 for eventCoroutine
			break;
		else
			eventCoroutine->resumeWithId((void*)ioEventDispatch);
	}

	keepRun = false;
	while (!signal) {
		eventCoroutine->resumeWithId((void*)ioEventDispatch);
		update();
	}
}

void CoroutineScheduler::stop()
{
	if (_CoroutineSchedulerInitCount > 0) {
		if(--_CoroutineSchedulerInitCount)
			return;
	}

	requestStop();

	FrameRateRegulator regulator;
	regulator.setTargetFraemRate(20);

	while(taskCount() > 0) {
		regulator.beginTask();
		update(unsigned(1000.0f * regulator.targetFrameTime));
		float elapsed, timeRemain;
		regulator.endTask(elapsed, timeRemain);

		if(taskCount() == _backgroundList.size())
			_bgKeepRun = false;
		else {
			int millSec = int(timeRemain * 1000);
			if(millSec > 0)
				TaskPool::sleep(millSec);
		}
	}

	ioEventShutdown();
}

bool CoroutineScheduler::keepRun() const
{
	return _keepRun;
}

bool CoroutineScheduler::bgKeepRun() const
{
	return _bgKeepRun;
}

roSize CoroutineScheduler::taskCount() const
{
	return _resumeList.size() + _suspendedList.size() + _resumeNextFrameList.size();
}

roSize CoroutineScheduler::activetaskCount() const
{
	return _resumeList.size();
}

roSize CoroutineScheduler::suspendedtaskCount() const
{
	return _suspendedList.size() + _resumeNextFrameList.size();
}

roUint64 CoroutineScheduler::currentTimeInTicks() const
{
	if(!_currentCoSleepManager)
		return 0;
	return _currentCoSleepManager->stopWatch.getTick();
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

//////////////////////////////////////////////////////////////////////////
// Coroutine

Coroutine::Coroutine()
	: _isInRun(false)
	, _isActive(false)
	, _retValueForSuspend(NULL)
	, _suspenderId(NULL)
	, _profilerNode(NULL)
	, _runningThreadId(0)
	, scheduler(NULL)
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
	return _ownStack.resizeNoInit(stackSize + _stackPadding);
}

void Coroutine::yield()
{
	roAssert(_isActive);
	scheduler->_resumeList.pushBack(*this);
	suspend();
}

void Coroutine::yieldFrame()
{
	roAssert(_isActive);
	scheduler->_resumeNextFrameList.pushBack(*this);
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

void* Coroutine::suspend()
{
	roAssert(_isInRun);
	roAssert(_isActive);
	roAssert(scheduler);
	roAssert(_runningThreadId == TaskPool::threadId());

	_isActive = false;
	
	// Check if it's already pending for resume (happens in yield)
	if(getList() != &scheduler->_resumeList && getList() != &scheduler->_resumeNextFrameList)
		scheduler->_suspendedList.pushBack(*this);

#if !CORO_FIBER
	if(_ownStack.isEmpty()) {
		// See how much stack we are using
		roByte* currentStack = (roByte*)_getCurrentStackPtr();
		roSize stackUsed = (&scheduler->_stack.back()) - currentStack;

		// Backup the current stack
		_backupStack.assign((&scheduler->_stack.back()) - stackUsed, stackUsed);
	}
#endif

	{	// Dummy profile scope to count yield time
		roScopeProfile("");
		coro_transfer(&_context, &scheduler->context);
	}

	void* ret = _retValueForSuspend;
	_retValueForSuspend = NULL;
	return ret;
}

void Coroutine::resume(void* retValueForSuspend)
{
	roAssert(_isInRun);
	roAssert(scheduler);

	// Put this Coroutine back to schedule
	if(!_isActive)
		scheduler->_resumeList.moveBack(*this);

	_retValueForSuspend = retValueForSuspend;
}

void* Coroutine::suspendWithId(void* id)
{
	_suspenderId = id;
	return suspend();
}

void Coroutine::resumeWithId(void* id, void* retValueForSuspend)
{
	roAssert(_suspenderId == id);
	resume(retValueForSuspend);
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

bool Coroutine::isSuspended() const
{
	return !_isActive;
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
#include <setjmp.h>

int roSetJmp(jmp_buf buf)
{
	return setjmp(buf);
}

void roLongJmp(jmp_buf buf, int val)
{
	longjmp(buf, val);
}
#endif

#include "roCoroutine.iocp.inc"
//#include "roCoroutine.select.inc"