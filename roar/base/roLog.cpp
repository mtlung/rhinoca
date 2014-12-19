#include "pch.h"
#include "roLog.h"
#include <stdio.h>

static void _defaultLogFunc(const char* type, const char* format, va_list ap)
{
	// TODO: These are 2 separate steps, not good when roLog is invoked in multiple threads
	if(type && type[0] != '\0')
		printf("%s: ", type);
	vprintf(format, ap);
}

static RoLogFunc _logFunc = _defaultLogFunc;

void roLog(const char* type, roPRINTF_FORMAT_PARAM const char* format, ...) roPRINTF_ATTRIBUTE(2,3)
{
	if(!format || !_logFunc) return;

	va_list vl;
	va_start(vl, format);
	_logFunc(type, format, vl);
	va_end(vl);
}

void roLogIf(bool shouldLog, const char* type, roPRINTF_FORMAT_PARAM const char* format, ...) roPRINTF_ATTRIBUTE(3,4)
{
	if(!shouldLog || !format || !_logFunc) return;

	va_list vl;
	va_start(vl, format);
	_logFunc(type, format, vl);
	va_end(vl);
}

void roSetLogFunc(RoLogFunc func)
{
	_logFunc = func;
}

RoLogFunc roGetLogFunc()
{
	return _logFunc;
}
