#ifndef __roCondVar_h__
#define __roCondVar_h__

#include "roMutex.h"
#include "../platform/roOS.h"

namespace ro {

/// Condition variable.
/// Conditions (also known as condition queues or condition variables) provide
/// a means for one thread to suspend execution (to "wait") until notified by
/// another thread that some state condition may now be true.
/// 
/// Because access to this shared state information occurs in different threads,
/// it must be protected, so a lock of some form is associated with the condition.
/// The key property that waiting for a condition provides is that it atomically
/// releases the associated lock and suspends the current thread.
/// 
/// A CondVar instance is intrinsically bound to a Mutex.
struct CondVar : public Mutex
{
	CondVar();
	~CondVar();

// Operations
	/// Causes the current thread to wait until it is signaled.
	/// The lock associated with this Condition is atomically released and the
	/// current thread becomes disabled for thread scheduling purposes and lies
	/// dormant until one of the following happens:
	///		-Some other thread invokes the signal() method for this CondVar and
	///		 the current thread happens to be chosen as the thread to be awakened; or
	///		-Some other thread invokes the SignalAll() method for this CondVar.
	/// \note The implementation (especially on windows) is subject to "Spurious wakeup",
	/// see http://en.wikipedia.org/wiki/Spurious_wakeup for details.
	void wait		();
	void waitNoLock	();

	/// Wait with timeout.
	/// return True if signaled otherwise false when timeout.
	bool wait		(unsigned timeoutInMs);
	bool waitNoLock	(unsigned timeoutInMs);

	/// Wakes up one waiting thread.
	/// If any threads are waiting on this condition then one is selected for
	/// waking up. That thread must then re-acquire the lock before returning from await.
	void signal		();

	/// Wakes up all waiting threads.
	/// If any threads are waiting on this condition then they are all woken up.
	/// Each thread must re-acquire the lock before it can return from await.
	void broadcast	();

// Private
#if roOS_WIN_MIN_SUPPORTED >= roOS_WIN_VISTA
	roPtrInt _cd;		// Win32 condition variable
#elif roCOMPILER_VC
	void* h[2];     // h[0]:signal, h[1]:broadcast
	int _waitCount;
	int _broadcastCount;
#elif roUSE_PTHREAD
	bool _waitNoLock(useconds_t microseconds);
	pthread_cond_t c;
#else
#	error
#endif
};	// Mutex

}	// namespace ro

#endif	// __roCondVar_h__
