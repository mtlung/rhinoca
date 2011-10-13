#ifndef __RHLOG_H__
#define __RHLOG_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#if _MSC_VER >= 1500

void rhLog(const char* type, _In_z_ _Printf_format_string_ const char* format, ...);

#elif __GNUC__ >= 4

void rhLog(const char* type, const char* format, ...) __attribute__((format(printf, 2, 3)));

#else

void rhLog(const char* type, const char* format, ...);

#endif

typedef void (*RhLogFunc)(const char* type, const char* format, va_list ap);

void rhSetLogFunc(RhLogFunc func);

RhLogFunc rhGetLogFunc();

#ifdef __cplusplus
}
#endif

#endif	// __RHLOG_H__
