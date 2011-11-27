#ifndef __roAtomic_h__
#define __roAtomic_h__

#include "../platform/roCompiler.h"

namespace ro {

///	An atomic integer class for performing increment and decrement operations.
struct AtomicInteger
{
public:
	AtomicInteger(roInt32 i=0)		{ _value = i; }

	/// Atomically pre-increment
	inline int	operator++();

	/// Atomically post-increment
	int			operator++(int)		{ return ++(*this) - 1; }

	/// Atomically pre-decrement
	inline int	operator--();

	/// Atomically post-decrement
	int			operator--(int)		{ return --(*this) + 1; }

	operator	int() const			{ return _value; }

	volatile roInt32 _value;
};	// AtomicInteger

}	// namespace ro

// ----------------------------------------------------------------------

#if roCOMPILER_VC
// Replace the huge <intrin.h> with just 4 lines of declaration
#if defined(__cplusplus)
extern "C" {
#endif
long __cdecl _InterlockedIncrement(long volatile *);
long __cdecl _InterlockedDecrement(long volatile *);
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#if defined(__cplusplus)
}
#endif
#elif roOS_APPLE
#	include <libkern/OSAtomic.h>
#endif

namespace ro {

#if roCOMPILER_VC

int AtomicInteger::operator++()	{ return _InterlockedIncrement((long*)&_value); }
int AtomicInteger::operator--()	{ return _InterlockedDecrement((long*)&_value); }

#elif roCOMPILER_GCC

// Reference: http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html#Atomic-Builtins
// Reference: http://golubenco.org/2007/06/14/atomic-operations
int AtomicInteger::operator++()
{
#if roOS_APPLE
	return OSAtomicIncrement32(&_value);
#else
	return __sync_add_and_fetch((long*)&_value, 1);
#endif
}

int AtomicInteger::operator--()
{
#if roOS_APPLE
	return OSAtomicDecrement32(&_value);
#else
	return __sync_sub_and_fetch((long*)&_value, 1);
#endif
}

#endif

}	// namespace ro

#endif	// __roAtomic_h__
