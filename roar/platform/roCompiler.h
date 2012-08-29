#ifndef __roCompiler_h__
#define __roCompiler_h__

#ifdef	_MSC_VER
#	include "roCompiler.vc.h"
#endif

#ifdef __GNUC__
#	include "roCompiler.gcc.h"
#endif

#ifdef __APPLE__
#	include <Availability.h>
#	include <TargetConditionals.h>
#	define roOS_APPLE 1
#	if TARGET_OS_IPHONE
#		define roOS_iOS 1
#	endif
#	if TARGET_IPHONE_SIMULATOR
#		define roOS_iOS 1
#	endif
#	if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#		define roOS_iOS 1
#	endif
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
#	define roVerify(x) (void)(x)
#endif

#define roIgnoreRet(x) (x)

#define roStaticAssert(x) typedef char __static_assert_t[(x)]

// Types for 32/64 bits compatibility
template<int> struct _sizetSelector {};
template<> struct _sizetSelector<4> { typedef roInt32 _signed; typedef roUint32 _unsigned; };
template<> struct _sizetSelector<8> { typedef roInt64 _signed; typedef roUint64 _unsigned; };

typedef _sizetSelector<sizeof(void*)>::_unsigned roSize;
typedef _sizetSelector<sizeof(void*)>::_signed roPtrInt;

// Force inline
#ifndef roFORCEINLINE
#	define roFORCEINLINE inline
#endif

// Offset of
#define roOffsetof(s, m) (roSize)((roPtrInt)&(((s*)0)->m))
#define roMemberHost(s, m, pm) (s*)((char*)pm - roOffsetof(s, m))

// Namespace forward declaration
namespace ro {}

#endif	// __roCompiler_h__
