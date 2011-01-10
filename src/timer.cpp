#include "pch.h"
#include "timer.h"
#include "common.h"
#include "platform.h"
#include <math.h>

#ifdef RHINOCA_APPLE
#	include <mach/mach_time.h>
#elif defined(_WIN32)
#	define USE_RDTSC 1
#endif

#if USE_RDTSC
#	ifdef RHINOCA_VC
#		define RDTSC(low, high)	\
		__asm rdtsc				\
		__asm mov low, eax		\
		__asm mov high, edx
#	elif defined(MCD_GCC)
#		define RDTSC(low, high)	\
		__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#	else
#	endif
#endif

// mftbu in PowerPC: http://lists.apple.com/archives/mac-games-dev/2002/May/msg00244.html
#if USE_RDTSC
inline rhuint64 rdtsc() {
	rhuint32 l, h;
	RDTSC(l, h);
	return (rhuint64(h) << 32) + l;
}
#endif

#if defined(_WIN32) || defined(RHINOCA_APPLE)

// Number of unit in one second.
rhuint64 getQueryPerformanceFrequency()
{
#if USE_RDTSC
	LARGE_INTEGER ret;
	VERIFY(::QueryPerformanceFrequency(&ret));
	ASSERT(sizeof(rhuint64) == sizeof(ret.QuadPart));

	// Try to get the ratio between QueryPerformanceCounter and rdtsc
	rhuint64 ticks1;
	::QueryPerformanceCounter((LARGE_INTEGER*)(&ticks1));
	rhuint64 ticks2 = rdtsc();

	// In most cases, the absolution values of QueryPerformanceCounter and rdtsc
	// can be used to computer their clock frequency ratio. But in some cases, for
	// example the machine is waken up from a sleeping state, the CPU clock is stoped
	// while the mother board's clock is still running, making them out of sync after
	// the machine wake up.
	double ratio = double(ticks2) / ticks1;
	if(true || ratio < 1) {	// Absolute value messed up, calcuate relative value.
		double dummy = double(ret.LowPart);
		for(int i=0; i<1000; ++i)
			dummy = sin(dummy);
		rhuint64 ticks1b;
		::QueryPerformanceCounter((LARGE_INTEGER*)(&ticks1b));
		rhuint64 ticks2b = rdtsc();
		ticks1 = ticks1b - ticks1;
		ticks2 = ticks2b - ticks2;
		// NOTE: The action of adding "dummy" to the equation is to
		// prevent the compiler optimize away the sin(dummy) operation.
		// And since the value of sin() always smaller than 1, so the error
		// introduced is minimum.
		ratio = double(ticks2 + dummy) / (ticks1);
	}

	return rhuint64(ret.QuadPart * ratio);
#elif defined(RHINOCA_APPLE)
	// Reference: http://www.macresearch.org/tutorial_performance_and_time
	// Reference: http://developer.apple.com/mac/library/qa/qa2004/qa1398.html
	mach_timebase_info_data_t info;
	if(mach_timebase_info(&info) == 0)
		return rhuint64(1e9 * (double) info.denom / (double) info.numer);
	else
		return 0;
#else
	return 0;
#endif
}

static const rhuint64 ticksPerSecond = getQueryPerformanceFrequency();
static const float invTicksPerSecond = 1.0f / ticksPerSecond;

float Timer::ticksToSeconds(rhuint64 ticks)
{
	return ticks * invTicksPerSecond;
}

rhuint64 Timer::secondsToTicks(float seconds)
{
	return rhuint64(seconds * ticksPerSecond);
}

#else

float Timer::ticksToSeconds(rhuint64 ticks)
{
	return float(double(ticks) * 1e-6);
}

rhuint64 Timer::secondsToTicks(float seconds)
{
	return rhuint64(seconds * 1e6);
}

#endif	// _WIN32

Timer::Timer() {
	reset();
}

rhuint64 Timer::ticks() const
{
	return ticksSinceProgramStatup() - _startTime;
}

float Timer::seconds() const
{
	return ticksToSeconds(ticks());
}

rhuint64 Timer::reset()
{
	rhuint64 backup = ticks();
	_startTime = ticksSinceProgramStatup();
	return backup;
}

rhuint64 Timer::ticksSinceProgramStatup()
{
	rhuint64 ret;

#if USE_RDTSC
	ret = rdtsc();
#elif defined(_WIN32)
	::QueryPerformanceCounter((LARGE_INTEGER*)(&ret));
#elif defined(RHINOCA_APPLE)
	ret = mach_absolute_time();
#else
	timeval tv;
	::gettimeofday(&tv, nullptr);
	ret = reinterpret_cast<const rhuint64&>(tv);
#endif

	return ret;
}
