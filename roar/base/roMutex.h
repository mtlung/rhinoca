#ifndef __roMutex_h__
#define __roMutex_h__

#include "roNonCopyable.h"
#include "../platform/roOS.h"
#include <type_traits>

#ifdef roUSE_PTHREAD
#	include <pthread.h>
#endif

#ifdef CONCURRENCYSAL_H
#   define roSAL_Lock(x) _Acquires_nonreentrant_lock_(x)
#   define roSAL_Unlock(x) _Releases_nonreentrant_lock_(x)
#   define roSAL_RecursiveLock(x) _Acquires_lock_(x)
#else
#   define roSAL_Lock(x)
#   define roSAL_Unlock(x)
#   define roSAL_RecursiveLock(x)
#endif

namespace ro {

struct Mutex : private NonCopyable
{
	/// Construct a mutex with an optional spin count.
	/// Use spinCount = 0 to disable spinning.
	explicit Mutex(unsigned spinCount = 200);
	~Mutex();

	roSAL_Lock(_mutex) void lock();
	roSAL_Unlock(_mutex) void unlock();
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

	roSAL_RecursiveLock(_mutex) void lock();
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

/// NullMutex
struct NullMutex : private NonCopyable
{
	void lock() {}
	void unlock() {}
	bool tryLock() { return false; }
};	// NullMutex


// ----------------------------------------------------------------------

/// Common class for some cancel-able classes.
struct Cancelable
{
	Cancelable() : _canceled(false) {}
	void cancel() { _canceled = true; }
	void resume() { _canceled = false; }
	bool isCanceled() const { return _canceled; }

	bool _canceled;
};	// Cancelable


// ----------------------------------------------------------------------

/*! Lock mutex in scope.
	Example:
	\code
	Mutex mutex;
	// ...
	{	ScopeLock<Mutex> lock(mutex);
		// We now protected by mutex, let's do something
		// ...
	}	// mutex get unlocked when out of scope
	\endcode
 */
template<class T>
struct ScopeLock : public Cancelable, private NonCopyable
{
	explicit ScopeLock(T& m) : Cancelable(), m(&m) { m.lock(); }
	explicit ScopeLock(T* m) : Cancelable(), m(m) { if(m) m->lock(); else cancel(); }
	~ScopeLock() { unlockAndCancel(); }
	void swapMutex(T& other) { m->unlock(); m = &other; m->lock(); }
	T& mutex() { return *m; }
	void unlockAndCancel() { if(!isCanceled()) m->unlock(); cancel(); }

	T* m;
};	// ScopeLock

/// Unlocking mutex in scope.
template<class T>
struct ScopeUnlock : public Cancelable, private NonCopyable
{
	explicit ScopeUnlock(T& m) : Cancelable(), m(&m) { m.unlock(); }
	explicit ScopeUnlock(T* m) : Cancelable(), m(m) { if(m) m->unlock(); else cancel(); }
	~ScopeUnlock() { lockAndCancel(); }
	T& mutex() { return *m; }
	void lockAndCancel() { if(!isCanceled()) m->lock(); cancel(); }

	T* m;
};	// ScopeUnlock

/// Unlocking mutex in scope.
/// \note Make sure the mutex is locked before ScopeUnlockOnly try to unlock it.
template<class T>
struct ScopeUnlockOnly : public Cancelable, private NonCopyable
{
	explicit ScopeUnlockOnly(T& m) : Cancelable(), m(&m) { }
	explicit ScopeUnlockOnly(T* m) : Cancelable(), m(m) { if(!m) cancel(); }
	~ScopeUnlockOnly() { if(!isCanceled()) m->unlock(); }
	T& mutex() { return *m; }

	T* m;
};	// ScopeUnlockOnly

}	// namespace ro

#endif	// __roMutex_h__

/// Macros to ease the use of ScopeLock family
#define roScopeLock(m)			::ro::ScopeLock<std::remove_reference_t<decltype(m)> >		roJoinMacro(scopeLock, __LINE__)(m);
#define roScopeUnlock(m)		::ro::ScopeUnlock<std::remove_reference_t<decltype(m)> >	roJoinMacro(scopeUnlock, __LINE__)(m);
#define roScopeUnlockOnly(m)	::ro::ScopeUnlockOnly<std::remove_reference_t<decltype(m)> >roJoinMacro(scopeUnlockOnly, __LINE__)(m);