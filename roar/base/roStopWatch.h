#ifndef __roStopWatch_h__
#define __roStopWatch_h__

#include "../platform/roCompiler.h"

namespace ro {

roUint64 ticksSinceProgramStatup();
roUint64 microSecondsSince1970();

double ticksToSeconds();
roUint64 secondsToTicks(double second);

/// A StopWatch to measure time interval.
struct StopWatch
{
	StopWatch();

	float	getFloat() const;
	double	getDouble() const;
	double	getAndReset();

	void	reset();
	void	resetToLastGet();

	void	pause();
	void	resume();

// Private
	mutable roUint64 _lastGetTime;	///< Record when the last get() is called (to prevent negative delta time)
	roUint64 _startTime;			///< Record when the StopWatch is created
	double _pauseAccumulate;
	bool _pause;
};	// StopWatch

}	// namespace ro

#endif	// __roStopWatch_h__
