#include "pch.h"
#include "roStopWatch.h"
#include "../platform/roPlatformHeaders.h"
#include <math.h>

#if roOS_iOS
#	include <mach/mach_time.h>
#elif roOS_WIN
#	define USE_RDTSC 1
#endif

#if USE_RDTSC

// mftbu in PowerPC: http://lists.apple.com/archives/mac-games-dev/2002/May/msg00244.html
inline roUint64 rdtsc() {
#	if roCOMPILER_VC
	return __rdtsc();
#else
	roUint32 l, h;
	__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
	return (roUint64(h) << 32) + l;
#endif
}

#endif

namespace ro {

#if roOS_WIN || roOS_iOS

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

	::Sleep(10);
	roUint64 ticks1b;
	::QueryPerformanceCounter((LARGE_INTEGER*)(&ticks1b));
	roUint64 ticks2b = rdtsc();
	ticks1 = ticks1b - ticks1;
	ticks2 = ticks2b - ticks2;

	double ratio = double(ticks2) / ticks1;
	return roUint64(ret.QuadPart * ratio);
#elif roOS_iOS
	// Reference: http://www.macresearch.org/tutorial_performance_and_time
	// Reference: http://developer.apple.com/mac/library/qa/qa2004/qa1398.html
	mach_timebase_info_data_t info;
	if(mach_timebase_info(&info) == 0)
		return roUint64(1e9 * (double) info.denom / (double) info.numer);
	else
		return 0;
#else
	LARGE_INTEGER ret;
	roVerify(::QueryPerformanceFrequency(&ret));

	return ret.QuadPart;
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
#elif roOS_iOS
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

float StopWatch::getFloat() const
{
	return (float)getDouble();
}

double StopWatch::getDouble() const
{
	if(_pause) return _pauseAccumulate;
	_lastGetTime = ticksSinceProgramStatup();
	return ticksToSeconds(_lastGetTime - _startTime) + _pauseAccumulate;
}

double StopWatch::getAndReset()
{
	double d = getDouble();
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
	_pauseAccumulate = getDouble();
	_pause = true;
}

void StopWatch::resume()
{
	if(!_pause) return;
	_pause = false;	
	_startTime = ticksSinceProgramStatup();
}

CountDownTimer::CountDownTimer(float timeOutInSecs)
{
	_beginTime = _stopWatch.getFloat();
	_endTime = _beginTime + timeOutInSecs;
	_numQuery = 0;
	_nextCheckAt = 0;
}

bool CountDownTimer::isExpired()
{
	++_numQuery;
	return _stopWatch.getFloat() >= _endTime;
}

bool CountDownTimer::isExpired(float& hint)
{
	if(++_numQuery < _nextCheckAt)
		return false;

	float current = _stopWatch.getFloat();
	float queryPerSec = _numQuery / (current - _beginTime);

	// Update the hint
	if(hint > queryPerSec)
		hint = hint * 0.9f + queryPerSec * 0.1f;	// Rise up the hint slowly
	else
		hint = hint * 0.5f + queryPerSec * 0.5f;	// Put down the hint fast

	_nextCheckAt = _numQuery + roSize(hint * (_endTime - _beginTime) * 0.2f);
	return current >= _endTime;
}

PeriodicTimer::PeriodicTimer(float period)
{
	reset(period);
}

void PeriodicTimer::reset()
{
	reset(_period);
}

void PeriodicTimer::reset(float period)
{
	_period = period;
	_stopWatch.reset();
	_getEventTime = _stopWatch.getFloat();
}

bool PeriodicTimer::isTriggered()
{
	float now = _stopWatch.getFloat();
	if(now > _getEventTime) {
		_getEventTime += _period;
		return true;
	}
	return false;
}

}	// namespace ro
