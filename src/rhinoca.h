#ifndef __RHINOCA_H__
#define __RHINOCA_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#	define RHINOCA_WINDOWS
#endif

#ifdef __APPLE__
#	include <Availability.h>
#	include <TargetConditionals.h>
#	define RHINOCA_APPLE
#	if TARGET_OS_IPHONE
#		define RHINOCA_IOS
#		define RHINOCA_IPHONE
#	endif
#	if TARGET_IPHONE_SIMULATOR
#		define RHINOCA_IOS
#		define RHINOCA_IPHONE_SIMULATOR
#	endif
#	if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#		define RHINOCA_IOS
#		define RHINOCA_IOS_DEVICE
#	endif
#endif

#ifdef _MSC_VER
#	define RHINOCA_VC
#ifndef _DEBUG
#	ifndef NDEBUG
#		define NDEBUG
#	endif
#endif
#endif

#ifdef __GNUC__
#	define RHINOCA_GCC
#endif

#ifndef RHINOCA_API
#	define RHINOCA_API extern
#endif

#ifdef _MSC_VER
typedef int rhint;
typedef __int8 rhint8;
typedef __int16 rhint16;
typedef __int32 rhint32;
typedef __int64 rhint64;
typedef unsigned int rhuint;
typedef unsigned __int8 rhuint8;
typedef unsigned __int16 rhuint16;
typedef unsigned __int32 rhuint32;
typedef unsigned __int64 rhuint64;
#else
typedef int rhint;
typedef char rhint8;
typedef short rhint16;
typedef int rhint32;
typedef long long rhint64;
typedef unsigned int rhuint;
typedef unsigned char rhuint8;
typedef unsigned short rhuint16;
typedef unsigned int rhuint32;
typedef unsigned long long rhuint64;
#endif
typedef rhuint8 rhbyte;

struct Rhinoca;
typedef struct Rhinoca Rhinoca;

/// Platform dependent handle
struct RhinocaRenderContext;
typedef struct RhinocaRenderContext RhinocaRenderContext;

/// Platform dependent key/mouse/gesture event data
struct RhinocaEvent {
	void* type;
	int value1;
	int value2;
	int value3;
	int value4;
};
typedef struct RhinocaEvent RhinocaEvent;

// Context management
RHINOCA_API void rhinoca_init();
RHINOCA_API void rhinoca_close();
RHINOCA_API Rhinoca* rhinoca_create(RhinocaRenderContext* renderContex);
RHINOCA_API void rhinoca_destroy(Rhinoca* rh);
RHINOCA_API void* rhinoca_getPrivate(Rhinoca* rh);
RHINOCA_API void rhinoca_setPrivate(Rhinoca* rh, void* data);

// Window
RHINOCA_API void rhinoca_setSize(Rhinoca* rh, rhuint width, rhuint height);
RHINOCA_API rhuint rhinoca_getWidth(Rhinoca* rh);
RHINOCA_API rhuint rhinoca_getHeight(Rhinoca* rh);

// Document open/close
RHINOCA_API void rhinoca_openDocument(Rhinoca* rh, const char* uri);
RHINOCA_API void rhinoca_closeDocument(Rhinoca* rh);

// Main loop iteration processing
RHINOCA_API void rhinoca_update(Rhinoca* rh);
RHINOCA_API void rhinoca_processEvent(Rhinoca* rh, RhinocaEvent ev);

// IO functions
typedef void* (*rhinoca_io_open)(Rhinoca* rh, const char* uri, int threadId);
typedef bool (*rhinoca_io_ready)(void* file, rhuint64 size, int threadId);
typedef rhuint64 (*rhinoca_io_read)(void* file, void* buffer, rhuint64 size, int threadId);
typedef int (*rhinoca_io_seek)(void* file, rhuint64 offset, int origin, int threadId);		/// Returns 1 for success, 0 for fail, -1 not supported. Origin: SEEK_SET, SEEK_CUR, SEEK_END.
typedef void (*rhinoca_io_close)(void* file, int threadId);
RHINOCA_API void rhinoca_io_setcallback(rhinoca_io_open open, rhinoca_io_ready ready, rhinoca_io_read read, rhinoca_io_seek seek, rhinoca_io_close close);
RHINOCA_API rhinoca_io_open rhinoca_get_io_open();
RHINOCA_API rhinoca_io_ready rhinoca_get_io_ready();
RHINOCA_API rhinoca_io_read rhinoca_get_io_read();
RHINOCA_API rhinoca_io_seek rhinoca_get_io_seek();
RHINOCA_API rhinoca_io_close rhinoca_get_io_close();

RHINOCA_API void* rhinoca_http_open(Rhinoca* rh, const char* uri, int threadId);
RHINOCA_API bool rhinoca_http_ready(void* file, rhuint64 size, int threadId);
RHINOCA_API rhuint64 rhinoca_http_read(void* file, void* buffer, rhuint64 size, int threadId);
RHINOCA_API void rhinoca_http_close(void* file, int threadId);

// Memory allocation
RHINOCA_API void* rhinoca_malloc(rhuint size);
RHINOCA_API void rhinoca_free(void* ptr);
RHINOCA_API void* rhinoca_realloc(void* ptr, rhuint oldSize, rhuint size);

// Others
typedef void (*rhinoca_printFunc)(Rhinoca* rh, const char* str, ...);
RHINOCA_API void rhinoca_setPrintFunc(rhinoca_printFunc printFunc);
typedef void (*rhinoca_alertFunc)(Rhinoca* rh, void* userData, const char* str);
RHINOCA_API void rhinoca_setAlertFunc(rhinoca_alertFunc alertFunc, void* userData);
RHINOCA_API void rhinoca_collectGarbage(Rhinoca* rh);

#ifdef __cplusplus
}
#endif

#endif
