#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include "rhinoca.h"

#if defined(RHINOCA_VC)
#	include <intrin.h>
#elif defined(RHINOCA_APPLE)
#	include <libkern/OSAtomic.h>
#endif

//!	An atomic integer class for performing increment and decrement operations.
class AtomicInteger
{
public:
	AtomicInteger(int i=0) { value=i; }

	//! Atomically pre-increment
	inline int operator++();

	//! Atomically post-increment
	int operator++(int) {
		return ++(*this) - 1;
	}

	//! Atomically pre-decrement
	inline int operator--();

	//! Atomically post-decrement
	int operator--(int) {
		return --(*this) + 1;
	}

	operator int() const {
		return value;
	}
private:
	volatile rhint32 value;
};	// AtomicInteger

#if defined(RHINOCA_VC)

int AtomicInteger::operator++() {
	return _InterlockedIncrement((long*)&value);
}

int AtomicInteger::operator--() {
	return _InterlockedDecrement((long*)&value);
}

#elif defined(RHINOCA_GCC)

// Reference: http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html#Atomic-Builtins
// Reference: http://golubenco.org/2007/06/14/atomic-operations
int AtomicInteger::operator++()
{
#ifdef RHINOCA_APPLE
	return OSAtomicIncrement32(&value);
#else
	return __sync_add_and_fetch((long*)&value, 1);
#endif
}

int AtomicInteger::operator--()
{
#ifdef RHINOCA_APPLE
	return OSAtomicDecrement32(&value);
#else
	return __sync_sub_and_fetch((long*)&value, 1);
#endif
}

#endif

#endif	// __ATOMIC_H__
