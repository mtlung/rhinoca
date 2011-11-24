#include "pch.h"
#include "roMutex.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

#ifndef roUSE_PTHREAD

Mutex::Mutex(int spinCount)
{
	// If you see this static assert, please check the size of the CRITICAL_SECTION
	roStaticAssert(sizeof(mMutex) == sizeof(CRITICAL_SECTION));

	// Fall back to InitializeCriticalSection if InitializeCriticalSectionAndSpinCount didn't success
	if(spinCount < 0 || !::InitializeCriticalSectionAndSpinCount((LPCRITICAL_SECTION)&mMutex, spinCount))
		::InitializeCriticalSection((LPCRITICAL_SECTION)&mMutex);

#if roDEBUG
	_locked = false;
#endif
}

Mutex::~Mutex()
{
#if roDEBUG
	roAssert(!_locked && "Delete before unlock");
#endif
	::DeleteCriticalSection((LPCRITICAL_SECTION)&mMutex);
}

void Mutex::lock()
{
	::EnterCriticalSection((LPCRITICAL_SECTION)&mMutex);
#if roDEBUG
	roAssert(!_locked && "Double lock");
	_locked = true;
#endif
}

void Mutex::unlock()
{
#if roDEBUG
	roAssert(_locked && "Unlock when not locked");
	_locked = false;
#endif
	::LeaveCriticalSection((LPCRITICAL_SECTION)&mMutex);
}

//! Return true if already locked, otherwise false
bool Mutex::tryLock()
{
	if(::TryEnterCriticalSection((LPCRITICAL_SECTION)&mMutex) > 0) {
#if roDEBUG
		roAssert(!_locked && "Double lock");
		_locked = true;
#endif
		return true;
	} else
		return false;
}

RecursiveMutex::RecursiveMutex(int spinCount)
{
	// If you see this static assert, please check the size of the CRITICAL_SECTION
	roStaticAssert(sizeof(mMutex) == sizeof(CRITICAL_SECTION));

	// Fall back to InitializeCriticalSection if InitializeCriticalSectionAndSpinCount didn't success
	if(spinCount < 0 || !::InitializeCriticalSectionAndSpinCount((LPCRITICAL_SECTION)&mMutex, spinCount))
		::InitializeCriticalSection((LPCRITICAL_SECTION)&mMutex);

#if roDEBUG
	_lockCount = 0;
#endif
}

RecursiveMutex::~RecursiveMutex()
{
#if roDEBUG
	roAssert(!isLocked() && "Delete before unlock");
#endif
	::DeleteCriticalSection((LPCRITICAL_SECTION)&mMutex);
}

void RecursiveMutex::lock()
{
	::EnterCriticalSection((LPCRITICAL_SECTION)&mMutex);
#if roDEBUG
	++_lockCount;
#endif
}

void RecursiveMutex::unlock()
{
#if roDEBUG
	roAssert(_lockCount > 0);
	--_lockCount;
#endif
	::LeaveCriticalSection((LPCRITICAL_SECTION)&mMutex);
}

bool RecursiveMutex::tryLock()
{
#if roDEBUG
	bool locked = ::TryEnterCriticalSection((LPCRITICAL_SECTION)&mMutex) > 0;
	if(locked) ++_lockCount;
	return locked;
#else
	return ::TryEnterCriticalSection((LPCRITICAL_SECTION)&mMutex) > 0;
#endif
}

#else

Mutex::Mutex(int spinCount)
{
#if roDEBUG
	_locked = false;
#endif
	// TODO: Spin count is not yet supported.
	::pthread_mutex_init(&mMutex, NULL);
}

Mutex::~Mutex()
{
#if roDEBUG
	roAssert(!_locked && "Delete before unlock");
#endif
	::pthread_mutex_destroy(&mMutex);
}

void Mutex::lock()
{
	roVerify(::pthread_mutex_lock(&mMutex) == 0);
#if roDEBUG
	roAssert(!_locked && "Double lock");
	_locked = true;
#endif
}

void Mutex::unlock()
{
#if roDEBUG
	roAssert(_locked && "Unlock when not locked");
	_locked = false;
#endif
	roVerify(::pthread_mutex_unlock(&mMutex) == 0);
}

bool Mutex::tryLock()
{
	if(!::pthread_mutex_trylock(&mMutex)) {
#if roDEBUG
		roAssert(!_locked && "Double lock");
		_locked = true;
#endif
		return true;
	} else {
		return false;
	}
}

RecursiveMutex::RecursiveMutex(int spinCount)
{
	// TODO: Support spin lock on Posix
	(void)spinCount;

	pthread_mutexattr_t attr;
	roVerify(::pthread_mutexattr_init(&attr) == 0);
	roVerify(::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0);
	roVerify(::pthread_mutex_init(&mMutex, &attr) == 0);
	roVerify(::pthread_mutexattr_destroy(&attr) == 0);

#if roDEBUG
	_lockCount = 0;
#endif
}

RecursiveMutex::~RecursiveMutex()
{
	roAssert(!isLocked() && "Delete before unlock");
	roVerify(::pthread_mutex_destroy(&mMutex) == 0);
}

void RecursiveMutex::lock()
{
	::pthread_mutex_lock(&mMutex);
#if roDEBUG
	++_lockCount;
#endif
}

void RecursiveMutex::unlock()
{
#if roDEBUG
	roAssert(_lockCount > 0);
	--_lockCount;
#endif
	roVerify(::pthread_mutex_unlock(&mMutex) == 0);
}

bool RecursiveMutex::tryLock()
{
#if roDEBUG
	bool locked = !::pthread_mutex_trylock(&mMutex);
	if(locked) ++_lockCount;
	return locked;
#else
	return !::pthread_mutex_trylock(&mMutex);
#endif
}

#endif	// MCD_WIN

#if roDEBUG
bool RecursiveMutex::isLocked() const
{
	return _lockCount > 0;
}
int RecursiveMutex::lockCount() const
{
	return _lockCount;
}
#endif

}	// namespace ro
