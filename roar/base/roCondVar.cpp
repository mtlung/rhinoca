#include "pch.h"
#include "roCondVar.h"
#include "roCpuProfiler.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

#if roCOMPILER_VC

CondVar::CondVar()
	: mWaitCount(0), mBroadcastCount(0)
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
	ScopeLock lock(*this);
	signalNoLock();
}

void CondVar::signalNoLock()
{
	roAssert(_locked);
	roVerify(::SetEvent(h[0]) != 0);
}

void CondVar::broadcast()
{
	ScopeLock lock(*this);
	broadcastNoLock();
}

void CondVar::broadcastNoLock()
{
	roAssert(_locked);
	roVerify(::SetEvent(h[1]) != 0);
	mBroadcastCount = mWaitCount;
}

void CondVar::wait()
{
	ScopeLock lock(*this);
	waitNoLock(INFINITE);
}

void CondVar::waitNoLock()
{
	waitNoLock(INFINITE);
}

#pragma warning(push)
#pragma warning(disable: 4702)
bool CondVar::waitNoLock(roUint32 milliseconds)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roAssert(_locked);
	++mWaitCount;

wait:
	Mutex::unlock();
	DWORD ret = ::WaitForMultipleObjects(2, h, false, milliseconds);
	Mutex::lock();
	--mWaitCount;

	switch(ret) {
	case WAIT_OBJECT_0:	return true;
	case WAIT_TIMEOUT:	return false;
	case WAIT_OBJECT_0+1:
		--mBroadcastCount;
		if(mBroadcastCount <= 0) {
			roVerify(::ResetEvent(h[1]) != 0);
			if(mBroadcastCount < 0)
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

void CondVar::signalNoLock()
{
#ifndef NDEBUG
	roAssert(_locked);
#endif
	::pthread_cond_signal(&c);
}

void CondVar::broadcastNoLock()
{
#ifndef NDEBUG
	roAssert(_locked);
#endif
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

}	// namespace ro
