#ifndef __TIMER_H__
#define __TIMER_H__

#include "rhinoca.h"

/// A timer to measure time interval.
class Timer
{
public:
	Timer();

	/// Get the time-elapsed since last reset
	rhuint64 ticks() const;
	float seconds() const;

	/// Reset the timer
	rhuint64 reset();

	static rhuint64 ticksSinceProgramStatup();
	static rhuint64 microSecondsSince1970();

	static float ticksToSeconds(rhuint64 ticks);
	static rhuint64 secondsToTicks(float seconds);

protected:
	mutable rhuint64 _lastGetTime;	///< Record when the last get() is called (to prevent negative delta time)
	rhuint64 _startTime;			///< Record when the timer is created
};	// Timer

//! A timer to measure delta time between each call.
class DeltaTimer
{
public:
	DeltaTimer();

	/// Since when the delta timer is called the first time, there is no delta (delta=0 or very very small)
	/// To prevent zero delta, the first delta can be specified in the constructor
	explicit DeltaTimer(float firstDelta);

	//! Get the delta time since last getDelta()
	float getDelta() const;

protected:
	Timer timer;
	mutable float lastTime;
};	// DeltaTimer

#endif	// __TIMER_H__
