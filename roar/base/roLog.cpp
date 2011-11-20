#include "pch.h"
#include "roLog.h"
#include <stdarg.h>
#include <stdio.h>

void _defaultLogFunc(const char* type, const char* format, va_list ap)
{
	// TODO: These are 2 seperate steps, not good when rhLog is invoked in multiple threads
	printf("%s: ", type);
	vprintf(format, ap);
}

static RoLogFunc _logFunc = _defaultLogFunc;

void roLog(const char* type, const char* format, ...)
{
	if(!format || !_logFunc) return;

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
