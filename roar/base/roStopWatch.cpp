#include "pch.h"
#include "roStopWatch.h"
#include "../platform/roPlatformHeaders.h"
#include <math.h>

#if RHINOCA_APPLE
#	include <mach/mach_time.h>
#elif roOS_WIN32
#	define USE_RDTSC 1
#endif

#if USE_RDTSC
#	if roCOMPILER_VC
#		define RDTSC(low, high)	\
		__asm rdtsc				\
		__asm mov low, eax		\
		__asm mov high, edx
#	elif roCOMPILER_GCC
#		define RDTSC(low, high)	\
		__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#	else
#	endif

// mftbu in PowerPC: http://lists.apple.com/archives/mac-games-dev/2002/May/msg00244.html
inline roUint64 rdtsc() {
	roUint32 l, h;
	RDTSC(l, h);
	return (roUint64(h) << 32) + l;
}

#endif

namespace ro {

#if roOS_WIN || RHINOCA_APPLE

// Number of unit in one second.
static roUint64 getQueryPerformanceFrequency()
{
#if USE_RDTSC
	LARGE_INTEGER ret;
	roVerify(::QueryPerformanceFrequency(&ret));
	roAssert(sizeof(roUint64) == sizeof(ret.QuadPart));

	// Try to get the ratio between QueryPerformanceCounter and rdtsc
	roUint64 ticks1;
	::QueryPerformanceCounter((LARGE_INTEGER*)(&ticks1));
	roUint64 ticks2 = rdtsc();

	// In most cases, the absolution values of QueryPerformanceCounter and rdtsc
	// can be used to computer their clock frequency ratio. But in some cases, for
	// example the machine is waken up from a sleeping state, the CPU clock is stopped
	// while the mother board's clock is still running, making them out of sync after
	// the machine wake up.
	double ratio = double(ticks2) / ticks1;
	if(ratio < 1) {	// Absolute value messed up, calculate relative value.
		double dummy = double(ret.LowPart);
		for(int i=0; i<1000; ++i)
			dummy = sin(dummy);
		roUint64 ticks1b;
		::QueryPerformanceCounter((LARGE_INTEGER*)(&ticks1b));
		roUint64 ticks2b = rdtsc();
		ticks1 = ticks1b - ticks1;
		ticks2 = ticks2b - ticks2;
		// NOTE: The action of adding "dummy" to the equation is to
		// prevent the compiler optimize away the sin(dummy) operation.
		// And since the value of sin() always smaller than 1, so the error
		// introduced is minimum.
		ratio = double(ticks2 + dummy) / (ticks1);
	}

	return roUint64(ret.QuadPart * ratio);
#elif defined(RHINOCA_APPLE)
	// Reference: http://www.macresearch.org/tutorial_performance_and_time
	// Reference: http://developer.apple.com/mac/library/qa/qa2004/qa1398.html
	mach_timebase_info_data_t info;
	if(mach_timebase_info(&info) == 0)
		return roUint64(1e9 * (double) info.denom / (double) info.numer);
	else
		return 0;
#else
	return 0;
#endif
}

static const roUint64 ticksPerSecond = getQueryPerformanceFrequency();
static const double invTicksPerSecond = 1.0f / ticksPerSecond;

double ticksToSeconds(roUint64 ticks)
{
	return ticks * invTicksPerSecond;
}

roUint64 secondsToTicks(float seconds)
{
	return roUint64(seconds * ticksPerSecond);
}

#else

double ticksToSeconds(roUint64 ticks)
{
	return double(ticks) * 1e-6;
}

roUint64 secondsToTicks(double seconds)
{
	return roUint64(seconds * 1e6);
}

#endif	// roOS_WIN

roUint64 ticksSinceProgramStatup()
{
	roUint64 ret;

#if USE_RDTSC
	ret = rdtsc();
#elif roOS_WIN
	::QueryPerformanceCounter((LARGE_INTEGER*)(&ret));
#elif RHINOCA_APPLE
	ret = mach_absolute_time();
#else
	timeval tv;
	::gettimeofday(&tv, nullptr);
	ret = reinterpret_cast<const roUint64&>(tv);
#endif

	return ret;
}

#if roOS_WIN

// Borrows from SpiderMonkey prmjtime.c
#define JSLL_INIT(hi, lo)  ((hi ## LL << 32) + lo ## LL)
#define FILETIME2INT64(ft) (((roUint64)ft.dwHighDateTime) << 32LL | (roUint64)ft.dwLowDateTime)
static const roUint64 win2un = JSLL_INIT(0x19DB1DE, 0xD53E8000);

roUint64 microSecondsSince1970()
{
	FILETIME ft;
	::GetSystemTimeAsFileTime(&ft);
	return (FILETIME2INT64(ft)-win2un)/10L;
}

#else

#include <sys/time.h>

roUint64 microSecondsSince1970()
{
    timeval tim;
    gettimeofday(&tim, NULL);
    
    return (roUint64)tim.tv_sec * 1000 + tim.tv_usec;
}

#endif

StopWatch::StopWatch()
: _pauseAccumulate(0), _pause(false)
{
	reset();
}

double StopWatch::get() const
{
	if(_pause) return _pauseAccumulate;
	_lastGetTime = ticksSinceProgramStatup();
	return ticksToSeconds(_lastGetTime - _startTime) + _pauseAccumulate;
}

double StopWatch::getAndReset()
{
	double d = get();
	resetToLastGet();
	return d;
}

void StopWatch::reset()
{
	_pauseAccumulate = 0;
	_startTime = ticksSinceProgramStatup();
	// NOTE: _lastGetTime will have a fresh value during the next call of get()
}

void StopWatch::resetToLastGet()
{
	_startTime = _lastGetTime;
	_pauseAccumulate = 0;
}

void StopWatch::pause()
{
	if(_pause) return;
	_pause = true;
	_pauseAccumulate = get();
}

void StopWatch::resume()
{
	if(!_pause) return;
	_pause = false;	
	_startTime = ticksSinceProgramStatup();
}

}	// namespace ro
