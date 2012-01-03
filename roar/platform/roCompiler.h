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

#if roDEBUG
#	define roVerify(x) roAssert(x)
#else
#	define roVerify(x) (x)
#endif

#define roIgnoreRet(x) (x)

#define roStaticAssert(x) typedef char __static_assert_t[(x)]

// Types for 32/64 bits compatibility
#ifndef roSize
#	define roSize size_t
#endif

#ifndef roPtrInt
#	define roPtrInt ptrdiff_t
#endif

// Force inline
#ifndef roFORCEINLINE
#	define roFORCEINLINE inline
#endif

// Offset of
#define roOffsetof(s, m) (roSize)((roPtrInt)&(((s*)0)->m))

#endif	// __roCompiler_h__
