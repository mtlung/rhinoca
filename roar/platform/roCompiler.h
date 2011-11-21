#ifndef __roCompiler_h__
#define __roCompiler_h__

#ifdef	_MSC_VER
#	include "roCompiler.vc.h"
#endif

#ifdef __GNUC__
#	include "roCompiler.gcc.h"
#endif

#ifndef NULL
#	define NULL 0
#endif

// Assertions
#ifndef roAssert
#	define roAssert(x) assert(x)
#endif

#ifdef _DEBUG
#	define roVerify(x) roAssert(x)
#else
#	define roVerify(x) (x)
#endif

#define roStaticAssert(x) typedef char __static_assert_t[(x)]

// Types for 32/64 bits compatibility
#ifndef roSize
#	define roSize size_t
#endif

#ifndef roPtrDiff
#	define roPtrDiff ptrdiff_t
#endif

#endif	// __roCompiler_h__
