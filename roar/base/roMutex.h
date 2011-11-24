#ifndef __roMutex_h__
#define __roMutex_h__

#include "roNonCopyable.h"
#include "../platform/roOS.h"

#ifdef roUSE_PTHREAD
#	include <pthread.h>
#endif

namespace ro {

class Mutex : private NonCopyable
{
public:
	/// Construct a mutex with an optional spin count.
	/// Use spinCount = -1 to disable spinning.
	explicit Mutex(int spinCount = 200);
	~Mutex();

	void lock();
	void unlock();
	bool tryLock();

#if roDEBUG
	/// For use in debug mode to assert that the mutex is locked.
	/// \note Do not assert for not locked, since the return value
	/// itself is not reliable if the mutex is not locked.
	bool isLocked() const { return _locked; }
#endif

#ifdef roUSE_PTHREAD
	pthread_mutex_t mMutex;
#else
	/// A char buffer that pretended to be a CRITICAL_SECTION.
	/// Using such an approach, we need not to include Windows.h
	/// The sizeof(CRITICAL_SECTION) is 24 on win32
	char mMutex[8 + 4 * sizeof(void*)];
#endif

#if roDEBUG
protected:
	bool _locked;
	char _padding[3];
#endif
};	// Mutex

/// RecursiveMutex
class RecursiveMutex : private NonCopyable
{
public:
	/// Construct a recursive mutex with an optional spin count.
	/// Use spinCount = -1 to disable spinning.
	explicit RecursiveMutex(int spintCount = 200);
	~RecursiveMutex();

	void lock();
	void unlock();
	bool tryLock();

#if roDEBUG
	/// For use in debug mode to assert that the mutex is locked.
	/// \note Do not assert for not locked, since the return value
	/// 	itself is not reliable if the mutex is not locked.
	bool isLocked() const;
	int lockCount() const;
#endif

#ifdef roUSE_PTHREAD
	pthread_mutex_t mMutex;
#else
	char mMutex[8 + 4 * sizeof(void*)];
#endif

#if roDEBUG
protected:
	int _lockCount;
#endif
};	// RecursiveMutex

/// Common class for some cancel-able classes.
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
class ScopeLock : public Cancelable, private NonCopyable
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

/// Unlocking mutex in scope.
class ScopeUnlock : public Cancelable, private NonCopyable
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

/// Unlocking mutex in scope.
/// \note Make sure the mutex is locked before ScopeUnlockOnly try to unlock it.
class ScopeUnlockOnly : public Cancelable, private NonCopyable
{
public:
	explicit ScopeUnlockOnly(Mutex& m) : Cancelable(), m(&m) { }
	explicit ScopeUnlockOnly(Mutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeUnlockOnly() { if(!isCanceled()) m->unlock(); }
	Mutex& mutex() { return *m; }

protected:
	Mutex* m;
};	// ScopeUnlockOnly

/// Lock recursive mutex in scope.
class ScopeRecursiveLock : public Cancelable, private NonCopyable
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

/// Unlocking recursive mutex in scope.
class ScopeRecursiveUnlock : public Cancelable, private NonCopyable
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

/// Unlocking recursive mutex in scope.
class ScopeRecursiveUnlockOnly : public Cancelable, private NonCopyable
{
public:
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex& m) : Cancelable(), m(&m) { }
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeRecursiveUnlockOnly() { if(!isCanceled()) m->unlock(); }
	RecursiveMutex& mutex() { return *m; }

protected:
	RecursiveMutex* m;
};	// ScopeRecursiveUnlockOnly

}	// namespace ro

#endif	// __roMutex_h__
