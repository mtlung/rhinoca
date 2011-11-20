#ifndef __roCompiler_h__
#define __roCompiler_h__

#ifdef	_MSC_VER
#	include "roCompiler.vc.h"
#endif

#ifdef __GNUC__
#	include "roCompiler.gcc.h"
#endif

// Assertions
#define roAssert(x) assert(x)
#ifdef _DEBUG
#	define roVerify(x) assert(x)
#else
#	define (x)
#endif

#define roStaticAssert(x) typedef char __static_assert_t[(x)]

// Types for 32/64 bits compatibility
#define roSize size_t
#define roPtrDiff ptrdiff_t

#endif	// __roCompiler_h__
