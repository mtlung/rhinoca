#ifndef __roAtomic_h__
#define __roAtomic_h__

#include "../platform/roCompiler.h"

long roAtomicInc(volatile long* v);
long roAtomicDec(volatile long* v);
long roAtomicAddThenFetch(volatile long* v, long toAdd);
long roAtomicCompareAndSwap(volatile long* v, long oldValue, long newValue);

// ----------------------------------------------------------------------

namespace ro {

///	An atomic integer class for performing increment and decrement operations.
struct AtomicInteger
{
	AtomicInteger(roInt32 i=0)		{ _value = i; }

	int			value() const		{ return roAtomicAddThenFetch((volatile long*)&_value, 0); }
	void		setValue(int v)		{ roAtomicCompareAndSwap(&_value, _value, v); }
	operator	int() const			{ return value(); }

	void operator=(int v)			{ setValue(v); }
	void operator=(AtomicInteger& v){ setValue(v); }

	int			operator++()		{ return roAtomicInc(&_value); }
	int			operator++(int)		{ return roAtomicInc(&_value) - 1; }
	int			operator--()		{ return roAtomicDec(&_value); }
	int			operator--(int)		{ return roAtomicDec(&_value) + 1; }

private:
	volatile long _value;
};	// AtomicInteger

/// Utility class to ease doing init/shutdown function
struct InitShutdownCounter
{
	bool tryInit()		{ return !(++_initCount > 1); }
	bool tryShutdown()	{ return !(_initCount == 0 || --_initCount > 0); }
	AtomicInteger _initCount;
};	// InitShutdownCounter

}	// namespace ro

// ----------------------------------------------------------------------

#if roCOMPILER_VC

// Replace the huge <intrin.h> with just 4 lines of declaration
#if defined(__cplusplus)
extern "C" {
#endif
long __cdecl _InterlockedIncrement(long volatile *);
long __cdecl _InterlockedDecrement(long volatile *);
long __cdecl _InterlockedExchangeAdd(long volatile *, long);
long __cdecl _InterlockedExchange(long volatile *, long);
long __cdecl _InterlockedCompareExchange(long volatile *, long, long);
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedCompareExchange)
#if defined(__cplusplus)
}
#endif

inline long roAtomicInc(volatile long* v)
{	return _InterlockedIncrement(v);	}

inline long roAtomicDec(volatile long* v)
{	return _InterlockedDecrement(v);	}

inline long roAtomicAddThenFetch(volatile long* v, long toAdd)
{	return _InterlockedExchangeAdd(v, toAdd);	}

inline long roAtomicCompareAndSwap(volatile long* v, long testValue, long newValue)
{	return _InterlockedCompareExchange(v, newValue, testValue);	}

#elif roOS_APPLE

#include <libkern/OSAtomic.h>

inline long roAtomicInc(volatile long* v)
{	return OSAtomicAdd32(v);	}

inline long roAtomicDec(volatile long* v)
{	return OSAtomicDecrement32(v);	}

inline long roAtomicAddThenFetch(volatile long* v, long toAdd)
{	return OSAtomicAdd32(toAdd, v);	}

inline long roAtomicCompareAndSwap(volatile long* v, long testValue, long newValue)
{	return OSAtomicCompareAndSwapLong(testValue, newValue, v);	}

#elif roCOMPILER_GCC

inline long roAtomicInc(volatile long* v)
{	return __sync_add_and_fetch(v, 1);	}

inline long roAtomicDec(volatile long* v)
{	return __sync_add_and_fetch(v, -1);	}

inline long roAtomicAddThenFetch(volatile long* v, long toAdd)
{	return __sync_add_and_fetch(v, toAdd);	}

inline long roAtomicCompareAndSwap(volatile long* v, long testValue, long newValue)
{	return __sync_val_compare_and_swap(v, testValue, newValue);	}

#endif

#endif	// __roAtomic_h__
