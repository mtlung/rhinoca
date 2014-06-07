#ifndef __roCompiler_h__
#define __roCompiler_h__

#if __cplusplus > 199711L
#	define roCPP11 1
#endif

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

#ifndef roDebugBreak
#	define roDebugBreak(x)
#endif

#ifndef roSetDataBreakpoint
#	define roSetDataBreakpoint(idx, addr, size)
#endif

#ifndef roRemoveDataBreakpoint
#	define roRemoveDataBreakpoint(idx)
#endif

#if roDEBUG
#	define roVerify(x) roAssert(x)
#	define roIsDebug (true)
#else
#	define roVerify(x) (void)(x)
#	define roIsDebug (false)
#endif

#define roIgnoreRet(x) (x)

#ifndef roStaticAssert
#	define roStaticAssert(x) typedef char __static_assert_t[(x)]
#endif

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
#ifndef roOffsetof
#	define roOffsetof(s, m) (roSize)((roPtrInt)&(((s*)0)->m))
#endif
#ifndef roContainerof
#	define roContainerof(s, m, pm) (s*)((char*)pm - roOffsetof(s, m))
#endif

// Counting the number of array elements
template<typename count, int size> 
char (*_roCountofHelper(count (&array)[size]))[size];
#define roCountof(array) ( sizeof(*_roCountofHelper(array)) )

#define roDelete(p) delete p, p = NULL

namespace ro {

// Make variable zero initialized by default
template<class T>
struct ZeroInit
{
	ZeroInit() : _m(0) {}
	ZeroInit(const T& t) : _m(t) {}

	ZeroInit&	operator=(const ZeroInit<T>& t)	{ _m = t; return *this; }
	T*			operator&()						{ return &_m; }
	const T*	operator&() const				{ return &_m; }
				operator T& ()					{ return _m; }
				operator const T& () const		{ return _m; }
	T _m;
};

// Make pointer variable NULL initialized by default
template<class T>
struct Ptr
{
	Ptr() : _ptr(NULL) {}
	Ptr(T* p) : _ptr(p) {}

	Ptr&		operator=(const Ptr<T>& t)		{ _ptr = t._ptr; return *this; }
				operator T* ()					{ return _ptr; }
				operator const T* () const		{ return _ptr; }
	T&			operator*()						{ return *_ptr; }
	T*			operator->()					{ return _ptr; }
	const T&	operator*() const				{ return *_ptr; }
	const T*	operator->() const				{ return _ptr; }
	T* _ptr;
};

}	// namespace ro

#endif	// __roCompiler_h__
