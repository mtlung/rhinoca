#ifndef __roMutex_h__
#define __roMutex_h__

#include "roNonCopyable.h"
#include "../platform/roOS.h"

#ifdef roUSE_PTHREAD
#	include <pthread.h>
#endif

namespace ro {

struct Mutex : private NonCopyable
{
	/// Construct a mutex with an optional spin count.
	/// Use spinCount = 0 to disable spinning.
	explicit Mutex(unsigned spinCount = 200);
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
	pthread_mutex_t _mutex;
#else
	/// A char buffer that pretended to be a CRITICAL_SECTION.
	/// Using such an approach, we need not to include Windows.h
	/// The sizeof(CRITICAL_SECTION) is 24 on win32
	char _mutex[8 + 4 * sizeof(void*)];
#endif

#if roDEBUG
	bool _locked;
	char _padding[3];
#endif
};	// Mutex

/// RecursiveMutex
struct RecursiveMutex : private NonCopyable
{
	/// Construct a recursive mutex with an optional spin count.
	/// Use spinCount = 0 to disable spinning.
	explicit RecursiveMutex(unsigned spintCount = 200);
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
	pthread_mutex_t _mutex;
#else
	char _mutex[8 + 4 * sizeof(void*)];
#endif

#if roDEBUG
	int _lockCount;
#endif
};	// RecursiveMutex


// ----------------------------------------------------------------------

/// Common class for some cancel-able classes.
struct Cancelable
{
	Cancelable() : _canceled(false) {}
	void cancel() { _canceled = true; }
	void resume() { _canceled = false; }
	bool isCanceled() const { return _canceled; }

	bool _canceled;
};	// RecursiveMutex


// ----------------------------------------------------------------------

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
struct ScopeLock : public Cancelable, private NonCopyable
{
public:
	explicit ScopeLock(Mutex& m) : Cancelable(), m(&m) { m.lock(); }
	explicit ScopeLock(Mutex* m) : Cancelable(), m(m) { if(m) m->lock(); else cancel(); }
	~ScopeLock() { unlockAndCancel(); }
	void swapMutex(Mutex& other) { m->unlock(); m = &other; m->lock(); }
	Mutex& mutex() { return *m; }
	void unlockAndCancel() { if(!isCanceled()) m->unlock(); cancel(); }

	Mutex* m;
};	// ScopeLock

/// Unlocking mutex in scope.
struct ScopeUnlock : public Cancelable, private NonCopyable
{
	explicit ScopeUnlock(Mutex& m) : Cancelable(), m(&m) { m.unlock(); }
	explicit ScopeUnlock(Mutex* m) : Cancelable(), m(m) { if(m) m->unlock(); else cancel(); }
	~ScopeUnlock() { lockAndCancel(); }
	Mutex& mutex() { return *m; }
	void lockAndCancel() { if(!isCanceled()) m->lock(); cancel(); }

	Mutex* m;
};	// ScopeUnlock

/// Unlocking mutex in scope.
/// \note Make sure the mutex is locked before ScopeUnlockOnly try to unlock it.
struct ScopeUnlockOnly : public Cancelable, private NonCopyable
{
	explicit ScopeUnlockOnly(Mutex& m) : Cancelable(), m(&m) { }
	explicit ScopeUnlockOnly(Mutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeUnlockOnly() { if(!isCanceled()) m->unlock(); }
	Mutex& mutex() { return *m; }

	Mutex* m;
};	// ScopeUnlockOnly

/// Lock recursive mutex in scope.
struct ScopeRecursiveLock : public Cancelable, private NonCopyable
{
	explicit ScopeRecursiveLock(RecursiveMutex& m) : Cancelable(), m(&m) { m.lock(); }
	explicit ScopeRecursiveLock(RecursiveMutex* m) : Cancelable(), m(m) { if(m) m->lock(); else cancel(); }
	~ScopeRecursiveLock() { unlockAndCancel(); }
	void swapMutex(RecursiveMutex& other) { m->unlock(); m = &other; m->lock(); }
	RecursiveMutex& mutex() { return *m; }
	void unlockAndCancel() { if(!isCanceled()) m->unlock(); cancel(); }

	RecursiveMutex* m;
};	// ScopeRecursiveLock

/// Unlocking recursive mutex in scope.
struct ScopeRecursiveUnlock : public Cancelable, private NonCopyable
{
	explicit ScopeRecursiveUnlock(RecursiveMutex& m) : Cancelable(), m(&m) { m.unlock(); }
	explicit ScopeRecursiveUnlock(RecursiveMutex* m) : Cancelable(), m(m) { if(m) m->unlock(); else cancel(); }
	~ScopeRecursiveUnlock() { lockAndCancel(); }
	RecursiveMutex& mutex() { return *m; }
	void lockAndCancel() { if(!isCanceled()) m->lock(); cancel(); }

	RecursiveMutex* m;
};	// ScopeRecursiveUnlock

/// Unlocking recursive mutex in scope.
struct ScopeRecursiveUnlockOnly : public Cancelable, private NonCopyable
{
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex& m) : Cancelable(), m(&m) { }
	explicit ScopeRecursiveUnlockOnly(RecursiveMutex* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeRecursiveUnlockOnly() { if(!isCanceled()) m->unlock(); }
	RecursiveMutex& mutex() { return *m; }

	RecursiveMutex* m;
};	// ScopeRecursiveUnlockOnly

}	// namespace ro

#endif	// __roMutex_h__
