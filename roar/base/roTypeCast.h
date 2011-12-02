#ifndef __roTypeCast_h__
#define __roTypeCast_h__

#include "roTypeOf.h"

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

inline bool roCastAssert(roUint16 from, roUint8)	{ return from <= ro::TypeOf<roUint8>::valueMax();}
inline bool roCastAssert(roUint16 from, roInt8)		{ return from <= (roUint16)ro::TypeOf<roInt8>::valueMax();}

template<class From, class To>
inline bool roCastAssert(From from, To)				{ return true; }

template<class Target, class Source>
inline Target num_cast(Source x)
{
	roAssert(roCastAssert(x, Target()));
	return static_cast<Target>(x);
}

#endif	// __roTypeCast_h__
