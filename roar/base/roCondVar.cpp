#include "pch.h"
#include "roCondVar.h"
#include "roCpuProfiler.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

#if roOS_WIN_MIN_SUPPORTED >= roOS_WIN_VISTA

CondVar::CondVar()
{
	// If you see this static assert, please check the size of the CONDITION_VARIABLE
	roStaticAssert(sizeof(_cd) == sizeof(CONDITION_VARIABLE));

	::InitializeConditionVariable(PCONDITION_VARIABLE(&_cd));
}

CondVar::~CondVar()
{
	// No win API to destroy condition variable... ??
}

void CondVar::signal()
{
	::WakeConditionVariable(PCONDITION_VARIABLE(&_cd));
}

void CondVar::broadcast()
{
	::WakeAllConditionVariable(PCONDITION_VARIABLE(&_cd));
}

bool CondVar::waitNoLock(roUint32 milliseconds)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

#if roDEBUG
	roAssert(_locked);
	_locked = false;
#endif

	bool ret = (::SleepConditionVariableCS(PCONDITION_VARIABLE(&_cd), (LPCRITICAL_SECTION)&_mutex, milliseconds) == TRUE);

#if roDEBUG
	_locked = true;
#endif

	return ret;
}

#elif roCOMPILER_VC

CondVar::CondVar()
	: _waitCount(0), _broadcastCount(0)
{
	// Create auto-reset
	h[0] = ::CreateEvent(NULL, false, false, NULL);
	roAssert(h[0]);
	// Create manual-reset
	h[1] = ::CreateEvent(NULL, true, false, NULL);
	roAssert(h[1]);
}

CondVar::~CondVar()
{
	if(h[0]) { roVerify(::CloseHandle(h[0]) != 0); h[0] = NULL; }
	if(h[1]) { roVerify(::CloseHandle(h[1]) != 0); h[1] = NULL; }
}

void CondVar::signal()
{
	roVerify(::SetEvent(h[0]) != 0);
}

void CondVar::broadcast()
{
	roVerify(::SetEvent(h[1]) != 0);
	_broadcastCount = _waitCount;
}

#pragma warning(push)
#pragma warning(disable: 4702)
bool CondVar::waitNoLock(roUint32 milliseconds)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roAssert(_locked);
	++_waitCount;

wait:
	Mutex::unlock();
	DWORD ret = ::WaitForMultipleObjects(2, h, false, milliseconds);
	Mutex::lock();
	--_waitCount;

	switch(ret) {
	case WAIT_TIMEOUT:	return false;
	case WAIT_OBJECT_0:	return true;
	case WAIT_OBJECT_0+1:
		--_broadcastCount;
		if(_broadcastCount <= 0) {
			roVerify(::ResetEvent(h[1]) != 0);
			if(_broadcastCount < 0)
				goto wait;
		}
		return true;
	case WAIT_ABANDONED:
	case WAIT_ABANDONED+1:
		roAssert(false && "CondVar abandon wait.");
		return false;
	default:
		roAssert(false);
		return false;
	}
}
#pragma warning(pop)

#elif roUSE_PTHREAD

CondVar::CondVar()
{
	roVerify(::pthread_cond_init(&c, NULL) == 0);
}

CondVar::~CondVar()
{
	roVerify(::pthread_cond_destroy(&c) == 0);
}

void CondVar::signal()
{
	::pthread_cond_signal(&c);
}

void CondVar::broadcast()
{
	::pthread_cond_broadcast(&c);
}

void CondVar::waitNoLock()
{
#ifndef NDEBUG
	roAssert(_locked);
	_locked = false;
#endif
	::pthread_cond_wait(&c, &_mutex);
#ifndef NDEBUG
	roAssert(!_locked);
	_locked = true;
#endif
}

bool CondVar::waitNoLock(roUint32 milliseconds)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

#ifndef NDEBUG
	roAssert(_locked);
	_locked = false;
#endif

	int ret;
	timespec t;
	timeval val;

	useconds_t microseconds = milliseconds 8 1000;

	::gettimeofday(&val, NULL);
	t.tv_sec =  val.tv_sec + time_t(microseconds / 1e6);
	t.tv_nsec = (microseconds % 1000000) * 1000;
	ret = ::pthread_cond_timedwait(&c, &_mutex, &t);
#ifndef NDEBUG
	// TODO: Consider error handling on ret != 0
	roAssert(!_locked);
	_locked = true;
#endif
	switch(ret) {
	case 0:			return true;
	case ETIMEDOUT:	return false;
	case EINVAL:
		roAssert(false && "Invalid param given to ::pthread_cond_timedwait");
		return false;
	default:
		roAssert(false);
		return false;
	}
}

#else
#	error
#endif

void CondVar::wait()
{
	roScopeLock(*this);
	waitNoLock(INFINITE);
}

void CondVar::waitNoLock()
{
	waitNoLock(INFINITE);
}

}	// namespace ro
