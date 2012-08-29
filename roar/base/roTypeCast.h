#ifndef __roTypeCast_h__
#define __roTypeCast_h__

#include "roTypeOf.h"

// ----------------------------------------------------------------------

inline bool roCastAssert(roUint64 from, roUint32)	{ return from <= ro::TypeOf<roUint32>::valueMax(); }
inline bool roCastAssert(roUint64 from, roUint16)	{ return from <= ro::TypeOf<roUint16>::valueMax(); }
inline bool roCastAssert(roUint64 from, roUint8)	{ return from <= ro::TypeOf<roUint8>::valueMax(); }
inline bool roCastAssert(roUint64 from, roInt32)	{ return from <= (roUint64)ro::TypeOf<roInt32>::valueMax(); }
inline bool roCastAssert(roUint64 from, roInt16)	{ return from <= (roUint64)ro::TypeOf<roInt16>::valueMax(); }
inline bool roCastAssert(roUint64 from, roInt8)		{ return from <= (roUint64)ro::TypeOf<roInt8>::valueMax(); }

inline bool roCastAssert(roUint32 from, roUint16)	{ return from <= ro::TypeOf<roUint16>::valueMax(); }
inline bool roCastAssert(roUint32 from, roUint8)	{ return from <= ro::TypeOf<roUint8>::valueMax(); }
inline bool roCastAssert(roUint32 from, roInt16)	{ return from <= (roUint32)ro::TypeOf<roInt16>::valueMax(); }
inline bool roCastAssert(roUint32 from, roInt8)		{ return from <= (roUint32)ro::TypeOf<roInt8>::valueMax(); }

inline bool roCastAssert(roUint16 from, roUint8)	{ return from <= ro::TypeOf<roUint8>::valueMax(); }
inline bool roCastAssert(roUint16 from, roInt8)		{ return from <= (roUint16)ro::TypeOf<roInt8>::valueMax(); }


inline bool roCastAssert(roInt32 from, roUint64)	{ return from >= 0; }
inline bool roCastAssert(roInt32 from, roUint32)	{ return from >= 0; }

inline bool roCastAssert(roInt16 from, roUint64)	{ return from >= 0; }
inline bool roCastAssert(roInt16 from, roUint32)	{ return from >= 0; }
inline bool roCastAssert(roInt16 from, roUint16)	{ return from >= 0; }

template<class From, class To>
inline bool roCastAssert(From from, To)				{ return true; }

template<class To, class From>
inline bool roCastAssert(From from)					{ To to = 0; return roCastAssert(from, to); }


// ----------------------------------------------------------------------

inline void roClampCast(roUint64 from, roUint64& to)	{ to = (roUint64)from; }
inline void roClampCast(roUint64 from, roUint32& to)	{ to = from > ro::TypeOf<roUint32>::valueMax() ? ro::TypeOf<roUint32>::valueMax() : (roUint32)from; }
inline void roClampCast(roUint64 from, roUint16& to)	{ to = from > ro::TypeOf<roUint16>::valueMax() ? ro::TypeOf<roUint16>::valueMax() : (roUint16)from; }
inline void roClampCast(roUint64 from, roUint8& to)		{ to = from > ro::TypeOf<roUint8>::valueMax() ? ro::TypeOf<roUint8>::valueMax() : (roUint8)from; }
inline void roClampCast(roUint64 from, roInt64& to)		{ to = from > (roUint64)ro::TypeOf<roInt64>::valueMax() ? ro::TypeOf<roInt64>::valueMax() : (roInt64)from; }
inline void roClampCast(roUint64 from, roInt32& to)		{ to = from > (roUint64)ro::TypeOf<roInt32>::valueMax() ? ro::TypeOf<roInt32>::valueMax() : (roInt32)from; }
inline void roClampCast(roUint64 from, roInt16& to)		{ to = from > (roUint64)ro::TypeOf<roInt16>::valueMax() ? ro::TypeOf<roInt16>::valueMax() : (roInt16)from; }
inline void roClampCast(roUint64 from, roInt8& to)		{ to = from > (roUint64)ro::TypeOf<roInt8>::valueMax() ? ro::TypeOf<roInt8>::valueMax() : (roInt8)from; }

inline void roClampCast(roUint32 from, roUint64& to)	{ to = (roUint64)from; }
inline void roClampCast(roUint32 from, roUint32& to)	{ to = (roUint32)from; }
inline void roClampCast(roUint32 from, roUint16& to)	{ to = from > ro::TypeOf<roUint16>::valueMax() ? ro::TypeOf<roUint16>::valueMax() : (roUint16)from; }
inline void roClampCast(roUint32 from, roUint8& to)		{ to = from > ro::TypeOf<roUint8>::valueMax() ? ro::TypeOf<roUint8>::valueMax() : (roUint8)from; }
inline void roClampCast(roUint32 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roUint32 from, roInt32& to)		{ to = from > (roUint32)ro::TypeOf<roInt32>::valueMax() ? ro::TypeOf<roInt32>::valueMax() : (roInt32)from; }
inline void roClampCast(roUint32 from, roInt16& to)		{ to = from > (roUint32)ro::TypeOf<roInt16>::valueMax() ? ro::TypeOf<roInt16>::valueMax() : (roInt16)from; }
inline void roClampCast(roUint32 from, roInt8& to)		{ to = from > (roUint32)ro::TypeOf<roInt8>::valueMax() ? ro::TypeOf<roInt8>::valueMax() : (roInt8)from; }

inline void roClampCast(roUint16 from, roUint64& to)	{ to = (roUint64)from; }
inline void roClampCast(roUint16 from, roUint32& to)	{ to = (roUint32)from; }
inline void roClampCast(roUint16 from, roUint16& to)	{ to = (roUint16)from; }
inline void roClampCast(roUint16 from, roUint8& to)		{ to = from > ro::TypeOf<roUint8>::valueMax() ? ro::TypeOf<roUint8>::valueMax() : (roUint8)from; }
inline void roClampCast(roUint16 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roUint16 from, roInt32& to)		{ to = (roInt32)from; }
inline void roClampCast(roUint16 from, roInt16& to)		{ to = from > (roUint16)ro::TypeOf<roInt16>::valueMax() ? ro::TypeOf<roInt16>::valueMax() : (roInt16)from; }
inline void roClampCast(roUint16 from, roInt8& to)		{ to = from > (roUint16)ro::TypeOf<roInt8>::valueMax() ? ro::TypeOf<roInt8>::valueMax() : (roInt8)from; }

inline void roClampCast(roUint8 from, roUint64& to)		{ to = (roUint64)from; }
inline void roClampCast(roUint8 from, roUint32& to)		{ to = (roUint32)from; }
inline void roClampCast(roUint8 from, roUint16& to)		{ to = (roUint16)from; }
inline void roClampCast(roUint8 from, roUint8& to)		{ to = (roUint8)from; }
inline void roClampCast(roUint8 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roUint8 from, roInt32& to)		{ to = (roInt32)from; }
inline void roClampCast(roUint8 from, roInt16& to)		{ to = (roInt16)from; }
inline void roClampCast(roUint8 from, roInt8& to)		{ to = from > ro::TypeOf<roInt8>::valueMax() ? ro::TypeOf<roInt8>::valueMax() : (roInt8)from; }

inline void roClampCast(roInt64 from, roUint64& to)		{ to = from < 0 ? 0 : (roUint64)from; }
inline void roClampCast(roInt64 from, roUint32& to)		{ if(from < 0) to = 0; else roClampCast((roUint64)from, to); }
inline void roClampCast(roInt64 from, roUint16& to)		{ if(from < 0) to = 0; else roClampCast((roUint64)from, to); }
inline void roClampCast(roInt64 from, roUint8& to)		{ if(from < 0) to = 0; else roClampCast((roUint64)from, to); }
inline void roClampCast(roInt64 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roInt64 from, roInt32& to)		{ roInt64 min=(roInt64)ro::TypeOf<roInt32>::valueMin(); roInt64 max=(roInt64)ro::TypeOf<roInt32>::valueMax(); to = roInt32(from > max ? max : (from < min ? min : from)); }
inline void roClampCast(roInt64 from, roInt16& to)		{ roInt64 min=(roInt64)ro::TypeOf<roInt16>::valueMin(); roInt64 max=(roInt64)ro::TypeOf<roInt16>::valueMax(); to = roInt16(from > max ? max : (from < min ? min : from)); }
inline void roClampCast(roInt64 from, roInt8& to)		{ roInt64 min=(roInt64)ro::TypeOf<roInt8>::valueMin(); roInt64 max=(roInt64)ro::TypeOf<roInt8>::valueMax(); to = roInt8(from > max ? max : (from < min ? min : from)); }

inline void roClampCast(roInt32 from, roUint64& to)		{ to = from < 0 ? 0 : (roUint64)from; }
inline void roClampCast(roInt32 from, roUint32& to)		{ to = from < 0 ? 0 : (roUint32)from; }
inline void roClampCast(roInt32 from, roUint16& to)		{ if(from < 0) to = 0; else roClampCast((roUint32)from, to); }
inline void roClampCast(roInt32 from, roUint8& to)		{ if(from < 0) to = 0; else roClampCast((roUint32)from, to); }
inline void roClampCast(roInt32 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roInt32 from, roInt32& to)		{ to = (roInt32)from; }
inline void roClampCast(roInt32 from, roInt16& to)		{ roInt32 min=(roInt32)ro::TypeOf<roInt16>::valueMin(); roInt32 max=(roInt32)ro::TypeOf<roInt16>::valueMax(); to = roInt16(from > max ? max : (from < min ? min : from)); }
inline void roClampCast(roInt32 from, roInt8& to)		{ roInt32 min=(roInt32)ro::TypeOf<roInt8>::valueMin(); roInt32 max=(roInt32)ro::TypeOf<roInt8>::valueMax(); to = roInt8(from > max ? max : (from < min ? min : from)); }

inline void roClampCast(roInt16 from, roUint64& to)		{ to = from < 0 ? 0 : (roUint64)from; }
inline void roClampCast(roInt16 from, roUint32& to)		{ to = from < 0 ? 0 : (roUint32)from; }
inline void roClampCast(roInt16 from, roUint16& to)		{ to = from < 0 ? 0 : (roUint16)from; }
inline void roClampCast(roInt16 from, roUint8& to)		{ if(from < 0) to = 0; else roClampCast((roUint16)from, to); }
inline void roClampCast(roInt16 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roInt16 from, roInt32& to)		{ to = (roInt32)from; }
inline void roClampCast(roInt16 from, roInt16& to)		{ to = (roInt16)from; }
inline void roClampCast(roInt16 from, roInt8& to)		{ roInt16 min=(roInt16)ro::TypeOf<roInt8>::valueMin(); roInt16 max=(roInt16)ro::TypeOf<roInt8>::valueMax(); to = roInt8(from > max ? max : (from < min ? min : from)); }

inline void roClampCast(roInt8 from, roUint64& to)		{ to = from < 0 ? 0 : (roUint64)from; }
inline void roClampCast(roInt8 from, roUint32& to)		{ to = from < 0 ? 0 : (roUint32)from; }
inline void roClampCast(roInt8 from, roUint16& to)		{ to = from < 0 ? 0 : (roUint16)from; }
inline void roClampCast(roInt8 from, roUint8& to)		{ to = from < 0 ? 0 : (roUint8)from; }
inline void roClampCast(roInt8 from, roInt64& to)		{ to = (roInt64)from; }
inline void roClampCast(roInt8 from, roInt32& to)		{ to = (roInt32)from; }
inline void roClampCast(roInt8 from, roInt16& to)		{ to = (roInt16)from; }
inline void roClampCast(roInt8 from, roInt8& to)		{ to = (roInt8)from; }

template<class Target, class Source>
inline Target num_cast(Source x)
{
	roAssert(roCastAssert(x, Target()));
	return static_cast<Target>(x);
}

template<class Target, class Source>
inline Target clamp_cast(Source x)
{
	Target ret;
	roClampCast(x, ret);
	return ret;
}

/// This cast treat the source as a block of memory
template<class Target, class Source>
inline Target mem_cast(Source x)
{
	roStaticAssert(sizeof(Target) == sizeof(Source));
	return *(Target*)(&x);
}


#endif	// __roTypeCast_h__
