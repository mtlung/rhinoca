#ifndef __roLog_h__
#define __roLog_h__

#include <stdarg.h>

#if _MSC_VER >= 1500

#include <sal.h>
void roLog(const char* type, _In_z_ _Printf_format_string_ const char* format, ...);

#elif __GNUC__ >= 4

void roLog(const char* type, const char* format, ...) __attribute__((format(printf, 2, 3)));

#else

void roLog(const char* type, const char* format, ...);

#endif

typedef void (*RoLogFunc)(const char* type, const char* format, va_list ap);

void roSetLogFunc(RoLogFunc func);

RoLogFunc roGetLogFunc();

#endif	// __roLog_h__
