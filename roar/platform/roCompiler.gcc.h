#include <stddef.h>

#define	roCOMPILER_GCC 1

typedef char roInt8;
typedef short roInt16;
typedef int roInt32;
typedef long long roInt64;
typedef unsigned char roUint8;
typedef unsigned short roUint16;
typedef unsigned int roUint32;
typedef unsigned long long roUint64;

typedef char roUtf8;
typedef unsigned short roUtf16;

#define roSize size_t

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

// Counting the number of array elements
#define roCountof(x) (sizeof(x) / sizeof(x[0]))

// Source code annotation
#if !defined(__midl) && defined(_PREFAST_) 
#	define override __declspec("__override")
#else
#	define override virtual
#endif

// Force inline
#define roFORCEINLINE inline __forceinline

// Printf style compiler check
#define roPRINTF_FORMAT_PARAM
#define roPRINTF_ATTRIBUTE(param1, param2) __attribute__((format(printf, param1, param2)))

#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_SECURE_NO_DEPRECATE
#	define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#	define _CRT_NONSTDC_NO_DEPRECATE
#endif
#pragma warning(disable : 4100)	// unreferenced formal parameter
#pragma warning(disable : 4127)	// conditional expression is constant
#pragma warning(disable : 4201)	// nonstandard extension used : nameless struct/union
#pragma warning(disable : 4514)	// unreferenced inline function has been removed
#pragma warning(disable : 4668)	// is not defined as a preprocessor macro
#pragma warning(disable : 4702)	// unreachable code
