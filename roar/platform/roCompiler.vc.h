#define	roCOMPILER_VC 1

typedef __int8 roInt8;
typedef __int16 roInt16;
typedef __int32 roInt32;
typedef __int64 roInt64;
typedef unsigned __int8 roUint8;
typedef unsigned __int16 roUint16;
typedef unsigned __int32 roUint32;
typedef unsigned __int64 roUint64;

#ifdef _M_IX86
#	define roCPU_x86 1
	typedef __w64 unsigned int _roSize;
	typedef __w64 int _roPtrInt;
#endif

#ifdef _M_X64
#	define roCPU_x86_64 1
	typedef unsigned __int64 _roSize;
	typedef __int64 _roPtrInt;
#endif

#define roSize _roSize
#define roPtrInt _roPtrInt

#ifdef _WIN64
#	define roOS_WIN64 1
#elif _WIN32
#	define roOS_WIN32 1
#endif

// Our own debug macro
#ifdef _DEBUG
#	define roDEBUG 1
#endif

// The inclusion of <assert.h> of VC was heavy, here we try to avoid that overhead
#if roDEBUG
#	define __roWIDEN(str) L ## str
#	define _roWIDEN(str) __roWIDEN(str)
#	define roAssert(expression) (void)( (!!(expression)) || (_roAssert(_roWIDEN(#expression), _roWIDEN(__FILE__), __LINE__), 0) )
	void _roAssert(const wchar_t* expression, const wchar_t* file, unsigned line);
#else
#	define roAssert(expression) ((void)0)
#endif

// Counting the number of array elements
#define roCountof(x) _countof(x)

// Source code annotation
#if !defined(__midl) && defined(_PREFAST_) 
#	define override __declspec("__override")
#else
#	define override virtual
#endif

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
#pragma warning(disable : 4100)	// unreferenced formal parameter
#pragma warning(disable : 4127)	// conditional expression is constant
#pragma warning(disable : 4514)	// unreferenced inline function has been removed
#pragma warning(disable : 4668)	// is not defined as a preprocessor macro
#pragma warning(disable : 4702)	// unreachable code
