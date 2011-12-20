#ifndef __roByteOrder_h__
#define __roByteOrder_h__

#include "../platform/roCompiler.h"
#include "../platform/roCpu.h"

#if roCOMPILER_VC
#include <intrin.h>
inline roUint8  roByteSwap(roUint8  x) { return x; }
inline roUint16 roByteSwap(roUint16 x) { return _byteswap_ushort(x); }
inline roUint32 roByteSwap(roUint32 x) { return _byteswap_ulong(x); }
inline roUint64 roByteSwap(roUint64 x) { return _byteswap_uint64(x); }
#elif roCOMPILER_GCC
inline roUint8  roByteSwap(roUint8  x) { return x; }
inline roUint16 roByteSwap(roUint16 x) { return (x>>8) | (x<<8); }
inline roUint32 roByteSwap(roUint32 x) { return (roUint32)__builtin_bswap32((roInt32)x); }
inline roUint64 roByteSwap(roUint64 x) { return (roUint64)__builtin_bswap64((roInt64)x); }
#else
inline roUint8  roByteSwap(roUint8  x) { return x; }
inline roUint16 roByteSwap(roUint16 x) { return (x>>8) | (x<<8); }
inline roUint32 roByteSwap(roUint32 x) {
	x= ((x<< 8)&0xFF00FF00UL) | ((x>> 8)&0x00FF00FFUL);
	return (x>>16) | (x<<16);
}

inline roUint64 roByteSwap( roUint64 x ) {
	x= ((x<< 8)&0xFF00FF00FF00FF00ULL) | ((x>> 8)&0x00FF00FF00FF00FFULL);
	x= ((x<<16)&0xFFFF0000FFFF0000ULL) | ((x>>16)&0x0000FFFF0000FFFFULL);
	return (x>>32) | (x<<32);
}
#endif

inline roInt8	roByteSwap(roInt8  x) { return (roInt8 )roByteSwap((roUint8 )x); }
inline roInt16	roByteSwap(roInt16 x) { return (roInt16)roByteSwap((roUint16)x); }
inline roInt32	roByteSwap(roInt32 x) { return (roInt32)roByteSwap((roUint32)x); }
inline roInt64	roByteSwap(roInt64 x) { return (roInt64)roByteSwap((roUint64)x); }

inline float	roByteSwap(float   x ) { roUint32 t; t = roByteSwap(*(roUint32*)&x); return *(float* )&t; }
inline double	roByteSwap(double  x ) { roUint64 t; t = roByteSwap(*(roUint64*)&x); return *(double*)&t; }

#ifdef roCPU_LITTLE_ENDIAN
#	define roChar4(a,b,c,d) (a | b<<8 | c<<16 | d<<24)
#	define roBYTE_ORDER 4321
#	define roTYPE_LIST(T) \
	inline T roHostToBE(T v) { return roByteSwap(v); } \
	inline T roHostToLE(T v) { return v; } \
	inline T roBEToHost(T v) { return roByteSwap(v); } \
	inline T roLEToHost(T v) { return v; } \

	roTYPE_LIST(roInt8)
	roTYPE_LIST(roInt16)
	roTYPE_LIST(roInt32)
	roTYPE_LIST(roInt64)
	roTYPE_LIST(roUint8)
	roTYPE_LIST(roUint16)
	roTYPE_LIST(roUint32)
	roTYPE_LIST(roUint64)
	roTYPE_LIST(float)
	roTYPE_LIST(double)
#	undef roTYPE_LIST
#endif	// roCPU_LITTLE_ENDIAN

#ifdef roCPU_BIG_ENDIAN
#	define roChar4(a,b,c,d) (a<<24 | b<<16 | c<<8 | d)
#	define roBYTE_ORDER 1234
#	define roTYPE_LIST(T) \
	inline T roHostToBE(T v) { return v; } \
	inline T roHostToLE(T v) { return roByteSwap(v); } \
	inline T roBEToHost(T v) { return v; } \
	inline T roLEToHost(T v) { return roByteSwap(v); } \

	roTYPE_LIST(roInt8)
	roTYPE_LIST(roInt16)
	roTYPE_LIST(roInt32)
	roTYPE_LIST(roInt64)
	roTYPE_LIST(roUint8)
	roTYPE_LIST(roUint16)
	roTYPE_LIST(roUint32)
	roTYPE_LIST(roUint64)
	roTYPE_LIST(float)
	roTYPE_LIST(double)
#	undef xTYPE_LIST
#endif	// roCPU_LITTLE_ENDIAN

#endif	// __roByteOrder_h__
