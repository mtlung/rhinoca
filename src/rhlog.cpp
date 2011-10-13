#include "pch.h"
#include "rhlog.h"
#include <stdarg.h>

void _defaultLogFunc(const char* type, const char* format, va_list ap)
{
	// TODO: These are 2 seperate steps, not good when rhLog is invoked in multiple threads
	printf("%s: ", type);
	vprintf(format, ap);
}

static RhLogFunc _logFunc = _defaultLogFunc;

void rhLog(const char* type, const char* format, ...)
{
	if(!format || !_logFunc) return;

	va_list vl;
	va_start(vl, format);
	_logFunc(type, format, vl);
	va_end(vl);
}

void rhSetLogFunc(RhLogFunc func)
{
	_logFunc = func;
}

RhLogFunc rhGetLogFunc()
{
	return _logFunc;
}
