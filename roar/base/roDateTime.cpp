#include "pch.h"
#include "roDateTime.h"
#include "roStringFormat.h"
#include "roUtility.h"
#include <time.h>

namespace ro {

DateTime::DateTime()
	: timeVal(0)
	, isGMT(false)
	, isConverted(false)
{
	roMemZeroStruct(convertedTime);
}

DateTime::DateTime(time_t t, bool gmt)
	: timeVal(t)
	, isGMT(gmt)
	, isConverted(false)
{
	roMemZeroStruct(convertedTime);
}

DateTime::DateTime(unsigned year, unsigned month, unsigned day, unsigned hour, unsigned minute, unsigned second, bool gmt)
	: isConverted(false)
{
	roMemZeroStruct(convertedTime);

	tm newTime;
	newTime.tm_year = year - 1900;
	newTime.tm_mon = month - 1;
	newTime.tm_mday = day;
	newTime.tm_hour = hour;
	newTime.tm_min = minute;
	newTime.tm_sec = second;

	isGMT = gmt;

	if(gmt) {
#if roCOMPILER_VC
		timeVal = _mkgmtime(&newTime);
#else
		// Without _mkgmtime, we approximate by calculating diff between local and gmt time
		// DST changes could cause some errors in the diff
		time_t localTime = mktime(&newTime);
		time_t diff = mktime(gmtime(&localTime)) - localTime;
		timeVal = (localTime + diff);
#endif
	}
	else
		timeVal = mktime(&newTime);
}

static tm& _convert(const DateTime& dateTime)
{
	if(!dateTime.isConverted)
	{
		time_t tmp = dateTime.timeVal;
		tm* timeptr = NULL;
		if(dateTime.isGMT)
			timeptr = gmtime(&tmp);
		else
			timeptr = localtime(&tmp);

		dateTime.convertedTime = *timeptr;
		dateTime.isConverted = true;
	}

	return dateTime.convertedTime;
}

DateTime DateTime::current(bool gmt)
{
	time_t t;
	time(&t);
	return DateTime(t, gmt);
}

unsigned DateTime::year() const {
	return _convert(*this).tm_year + 1900;
}

unsigned DateTime::month() const {
	return _convert(*this).tm_mon + 1;
}

unsigned DateTime::day() const {
	return _convert(*this).tm_mday;
}

unsigned DateTime::hour() const {
	return _convert(*this).tm_hour;
}

unsigned DateTime::minute() const {
	return _convert(*this).tm_min;
}

unsigned DateTime::second() const {
	return _convert(*this).tm_sec;
}

unsigned DateTime::yearDay() const {
	return _convert(*this).tm_yday;
}

unsigned DateTime::monthDay() const {
	return _convert(*this).tm_mday;
}

unsigned DateTime::weekDay() const {
	return _convert(*this).tm_wday;
}

static void _strFormat_DateTime(String& str, DateTime* val, const roUtf8* options)
{
	if(!val) return;

	strFormat(str, "{}/{}/{} - {}:{}:{}", val->day(), val->month(), val->year(), val->hour(), val->minute(), val->second());
}

roPtrInt _strFormatFunc(const DateTime& val) {
	return (roPtrInt)_strFormat_DateTime;
}

}	// namespace ro
