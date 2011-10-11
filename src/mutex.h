#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "rhinoca.h"
#include "rhassert.h"
#include "noncopyable.h"

#if !defined(RHINOCA_WINDOWS)
#	include <pthread.h>
#endif

class Mutex : private Noncopyable
{
public:
	/*!	Construct a mutex with an optional spin count.
		Use spinCount = -1 to disable spinning.
	 */
	explicit Mutex(int spinCount = 200);
	~Mutex();

	void lock();
	void unlock();
	bool tryLock();

#ifndef NDEBUG
	/*!	For use in debug mode to assert that the mutex is locked.
		\note Do not assert for not locked, since the return value
			itself is not reliable if the mutex is not locked.
	 */
	bool isLocked() const { return _locked; }
#endif

#ifdef RHINOCA_WINDOWS
	/*!	A char buffer that pretended to be a CRITICAL_SECTION.
		Using such an approach, we need not to include Windows.h
		The sizeof(CRITICAL_SECTION) is 24 on win32
	 */
	char mMutex[8 + 4 * sizeof(void*)];
#else
	pthread_mutex_t mMutex;
#endif

#ifndef NDEBUG
protected:
	bool _locked;
	char _padding[3];
#endif
};	// Mutex

//! RecursiveMutex
class RecursiveMutex : private Noncopyable
{
public:
	/*!	Construct a recursive mutex with an optional spin count.
		Use spinCount = -1 to disable spinning.
	 */
	explicit RecursiveMutex(int spintCount = 200);
	~RecursiveMutex();

	void lock();
	void unlock();
	bool tryLock();

#ifndef NDEBUG
	/*!	For use in debug mode to assert that the mutex is locked.
		\note Do not assert for not locked, since the return value
			itself is not reliable if the mutex is not locked.
	 */
	bool isLocked() const;
	int lockCount() const;
#endif

#ifdef RHINOCA_WINDOWS
	char mMutex[8 + 4 * sizeof(void*)];
#else
	pthread_mutex_t mMutex;
#endif

#ifndef NDEBUG
protected:
	int _lockCount;
#endif
};	// RecursiveMutex

//! Common class for some cancel-able classes.
class Cancelable {
public:
	Cancelable() : mCanceled(false) {}
	void cancel() { mCanceled = true; }
	void resume() { mCanceled = false; }
	bool isCanceled() const { return mCanceled; }

protected:
	bool mCanceled;
};	// RecursiveMutex

/*! Lock mutex in scope.
	Example:
	\code
	Mutex mutex;
	// ...
	{	ScopeLock lock(mutex);
		// We now protected by mutex, let's do something
		// ...
	}	// mutex get unlocked when out of scope
	\endcode
 */
class ScopeLock : public Cancelable, private Noncopyable
{
public:
	explicit ScopeLock(Mutex& m) : Cancelable(), m(&m) { m.lock(); }
	explicit ScopeLock(Mutex* m) : Cancelable(), m(m) { if(m) m->lock(); else cancel(); }
	~ScopeLock() { unlockAndCancel(); }
	void swapMutex(Mutex& other) { m->unlock(); m = &other; m->lock(); }
	Mutex& mutex() { return *m; }
	void unlockAndCancel() { if(!isCanceled()) m->unlock(); cancel(); }

protected:
	Mutex* m;
};	// ScopeLock

//! Unlocking mutex in scope.
class ScopeUnlock : public Cancelable, private Noncopyable
{
public:
	explicit ScopeUnlock(Mutex& m) : Cancelable(), m(&m) { m.unlock(); }
	explicit ScopeUnlock(Mutex* m) : Cancelable(), m(m) { if(m) m->unlock(); else cancel(); }
	~ScopeUnlock() { lockAndCancel(); }
	Mutex& mutex() { return *m; }
	void lockAndCancel() { if(!isCanceled()) m->lock(); cancel(); }

protected:
	Mutex* m;
};	// ScopeUnlock

/*! Unlocking mutex in scope.
	\note Make sure the mutex is locked before ScopeUnlockOnly try to unlock it.
 */
class ScopeUnlockOnly : public Cancelable, private Noncopyable
{
public:
	explicit ScopeUnlockOnly(Mutex& m) : Cancelable(), m(&m) { }
	explicit ScopeUnlockOnly(Mutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeUnlockOnly() { if(!isCanceled()) m->unlock(); }
	Mutex& mutex() { return *m; }

protected:
	Mutex* m;
};	// ScopeUnlockOnly

//! Lock recursive mutex in scope.
class ScopeRecursiveLock : public Cancelable, private Noncopyable
{
public:
	explicit ScopeRecursiveLock(RecursiveMutex& m) : Cancelable(), m(&m) { m.lock(); }
	explicit ScopeRecursiveLock(RecursiveMutex* m) : Cancelable(), m(m) { if(m) m->lock(); else cancel(); }
	~ScopeRecursiveLock() { unlockAndCancel(); }
	void swapMutex(RecursiveMutex& other) { m->unlock(); m = &other; m->lock(); }
	RecursiveMutex& mutex() { return *m; }
	void unlockAndCancel() { if(!isCanceled()) m->unlock(); cancel(); }

protected:
	RecursiveMutex* m;
};	// ScopeRecursiveLock

//! Unlocking recursive mutex in scope.
class ScopeRecursiveUnlock : public Cancelable, private Noncopyable
{
public:
	explicit ScopeRecursiveUnlock(RecursiveMutex& m) : Cancelable(), m(&m) { m.unlock(); }
	explicit ScopeRecursiveUnlock(RecursiveMutex* m) : Cancelable(), m(m) { if(m) m->unlock(); else cancel(); }
	~ScopeRecursiveUnlock() { lockAndCancel(); }
	RecursiveMutex& mutex() { return *m; }
	void lockAndCancel() { if(!isCanceled()) m->lock(); cancel(); }

protected:
	RecursiveMutex* m;
};	// ScopeRecursiveUnlock

//! Unlocking recursive mutex in scope.
class ScopeRecursiveUnlockOnly : public Cancelable, private Noncopyable
{
public:
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex& m) : Cancelable(), m(&m) { }
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeRecursiveUnlockOnly() { if(!isCanceled()) m->unlock(); }
	RecursiveMutex& mutex() { return *m; }

protected:
	RecursiveMutex* m;
};	// ScopeRecursiveUnlockOnly

#endif	// __MUTEX_H__
