#ifndef __roDateTime_h__
#define __roDateTime_h__

#include "../platform/roCompiler.h"
#include <time.h>

namespace ro {

/// If GMT is not specified, local time is used
/// GMT stands for Greenwich Mean Time
struct DateTime
{
	DateTime();
	DateTime(time_t t, bool gmt=false);
	DateTime(unsigned year, unsigned month, unsigned day, unsigned hour, unsigned minute, unsigned second, bool gmt = false);

	static DateTime current(bool gmt=false);

	unsigned year() const;
	unsigned month() const;
	unsigned day() const;		// Same as monthDay()
	unsigned hour() const;
	unsigned minute() const;
	unsigned second() const;

	unsigned yearDay() const;
	unsigned monthDay() const;
	unsigned weekDay() const;

// Private
	time_t			timeVal;
	bool			isGMT;
	mutable bool	isConverted;
	mutable tm		convertedTime;
};	// DateTime

void* _strFormatFunc(const DateTime& val);

}   // namespace ro

#endif	// __roDateTime_h__
