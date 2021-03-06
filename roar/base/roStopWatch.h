#ifndef __roStopWatch_h__
#define __roStopWatch_h__

#include "../platform/roCompiler.h"

namespace ro {

roUint64 ticksSinceProgramStatup();
roUint64 microSecondsSince1970();

double ticksToSeconds(roUint64 ticks);
roUint64 secondsToTicks(double second);

/// A StopWatch to measure time interval.
/// This timer is suppose to measure relative shot duration,
/// as this timer will diverge about 1ms every 1000s
struct StopWatch
{
	StopWatch();

	roUint64	getTick() const;
	float		getFloat() const;
	double		getDouble() const;
	double		getAndReset();

	void		reset();
	void		resetToLastGet();

	void		pause();
	void		resume();

// Private
	mutable roUint64	_lastGetTime;	///< Record when the last get() is called (to prevent negative delta time)
	roUint64			_timeAtResume;	///< Record when the StopWatch is created/resumed
	roUint64			_timeAtPause;
	bool				_pause;
};	// StopWatch

/// Similar to StopWatch but it counts multiple start() / stop() even they are overlapped.
/// This is very usefull when implementing CpuProfiler with Coroutine
struct MultiStartStopWatch
{
	MultiStartStopWatch();

	void		reset();
	void		start();
	float		getAndStop();
	float		getFloat() const;

// Private
	unsigned	_multiplier = 0;
	roUint64	_last = 0;
	roUint64	_duration = 0;
};	// MultiStartStopWatch

/// Utility to keep track a time out
struct CountDownTimer
{
	explicit CountDownTimer(float timeOutInSecs);

	bool isExpired();

	/// If we give some hint to CountDownTimer, we can reduce the invocation to StopWatch query
	/// The hint is the average time gap between each expected expiration query.
	bool isExpired(float& hint);

	void pause() { _stopWatch.pause(); }
	void resume() { _stopWatch.resume(); }

// Private
	float _beginTime;
	float _endTime;
	roSize _numQuery;
	roSize _nextCheckAt;
	StopWatch _stopWatch;
};	// CountDownTimer

/// Give you event at a fixed period
struct PeriodicTimer
{
	explicit PeriodicTimer(float period=0);

	/// If you invoke isTriggered() longer than the period, it will return true immediately.
	/// In other words, you will not missing any event.
	bool isTriggered();

	void reset();
	void reset(float period);
	void pause() { _stopWatch.pause(); }
	void resume() { _stopWatch.resume(); }

// Private
	float _period;
	float _getEventTime;	///< Increment on ever call to isTriggered()
	StopWatch _stopWatch;
};	// PeriodicTimer

/// Help to achieve a stable target fps by measure the time taken for a task,
/// and suggest the time remains until the frame end
struct FrameRateRegulator
{
	FrameRateRegulator();

	void	setTargetFraemRate(float targetFps);
	void	beginTask();
	void	endTask(float& elapsed, float& timeRemain);
	float	timeToExpectedFrameEnd() const;

	float		targetFrameTime;
	float		avgFrameTime;
	float		refinementFactor;
	StopWatch	fpsTimer;
	StopWatch	stopWatch;
};	// FrameRateRegulator

}	// namespace ro

#endif	// __roStopWatch_h__
