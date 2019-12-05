#define	roCOMPILER_VC 1

typedef __int8 roInt8;
typedef __int16 roInt16;
typedef __int32 roInt32;
typedef __int64 roInt64;
typedef unsigned __int8 roUint8;
typedef unsigned __int16 roUint16;
typedef unsigned __int32 roUint32;
typedef unsigned __int64 roUint64;

typedef char roUtf8;
typedef unsigned __int16 roUtf16;

typedef unsigned char roByte;

#ifdef _M_IX86
#	define roCPU_x86 1
#endif

#ifdef _M_X64
#	define roCPU_x86_64 1
#endif

#ifdef _WIN64
#	define roOS_WIN   1
#	define roOS_WIN64 1
#elif _WIN32
#	define roOS_WIN   1
#	define roOS_WIN32 1
#endif

// Our own debug macro
#ifdef _DEBUG
#	define roDEBUG 1
#endif

// SIMD
#define roCPU_SSE 1

// The inclusion of <assert.h> of VC was heavy, here we try to avoid that overhead
#if roDEBUG
#	define __roWIDEN(str) L ## str
#	define _roWIDEN(str) __roWIDEN(str)
#	define roAssert(expression) (void)( (!!(expression)) || (_roAssert(_roWIDEN(#expression), _roWIDEN(__FILE__), __LINE__), 0) )
	void _roAssert(const wchar_t* expression, const wchar_t* file, unsigned line);
#else
#	define roAssert(expression) {((void)0);}
#endif

// Break if debugger present
void _roDebugBreak(bool doBreak=true);
#define roDebugBreak _roDebugBreak

bool _roSetDataBreakpoint(unsigned index, void* address, unsigned size);
bool _roRemoveDataBreakpoint(unsigned index);
#define roSetDataBreakpoint _roSetDataBreakpoint
#define roRemoveDataBreakpoint _roRemoveDataBreakpoint

// Force inline
#define roFORCEINLINE __forceinline

// Printf style compiler check
#define roPRINTF_FORMAT_PARAM _In_z_ _Printf_format_string_
#define roPRINTF_ATTRIBUTE(param1, param2)

#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_SECURE_NO_DEPRECATE
#	define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#	define _CRT_NONSTDC_NO_DEPRECATE
#endif

#ifndef _ALLOW_RTCc_IN_STL	// STL in VS2015 Update2 has potential of truncating type
#	define _ALLOW_RTCc_IN_STL
#endif

#ifndef _CRT_NO_POSIX_ERROR_CODES	// Avoid bring in POSIX error codes in <errno.h>
#	define _CRT_NO_POSIX_ERROR_CODES
#endif

#pragma warning(disable : 4100)	// unreferenced formal parameter
#pragma warning(disable : 4127)	// conditional expression is constant
#pragma warning(disable : 4201)	// nonstandard extension used : nameless struct/union
#pragma warning(disable : 4514)	// unreferenced inline function has been removed
#pragma warning(disable : 4668)	// is not defined as a preprocessor macro
#pragma warning(disable : 4702)	// unreachable code
