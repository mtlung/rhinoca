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
#	define roSize __w64 unsigned int
#	define roPtrDiff __w64 int
#endif

#ifdef _M_X64
#	define roCPU_x86_64 1
#	define roSize unsigned __int64
#	define roPtrDiff __int64
#endif

#ifdef _WIN64
#	define roOS_WIN64 1
#elif _WIN32
#	define roOS_WIN32 1
#endif

// The inclusion of <assert.h> of VC was heavy, here we try to avoid that overhead
#ifdef _DEBUG
#	define __roWIDEN(str) L ## str
#	define _roWIDEN(str) __roWIDEN(str)
#	define roAssert(expression) (void)( (!!(expression)) || (_roAssert(_roWIDEN(#expression), _roWIDEN(__FILE__), __LINE__), 0) )
	void _roAssert(const wchar_t* expression, const wchar_t* file, unsigned line);
#else
#	define roAssert(expression) ((void)0)
#endif

// Counting the number of array elements
#define roCOUNTOF(x) _countof(x)

// Source code annotation
#if !defined(__midl) && defined(_PREFAST_) 
#	define override __declspec("__override")
#else
#	define override virtual
#endif

#pragma warning(disable : 4127)	// conditional expression is constant
