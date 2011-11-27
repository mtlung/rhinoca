#ifndef __roLog_h__
#define __roLog_h__

#include "../platform/roCompiler.h"
#include <stdarg.h>

#if roCOMPILER_VC
#	include <sal.h>
#endif

void roLog(const char* type, roPRINTF_FORMAT_PARAM const char* format, ...) roPRINTF_ATTRIBUTE(2,3);

typedef void (*RoLogFunc)(const char* type, const char* format, va_list ap);

void roSetLogFunc(RoLogFunc func);

RoLogFunc roGetLogFunc();

#endif	// __roLog_h__
