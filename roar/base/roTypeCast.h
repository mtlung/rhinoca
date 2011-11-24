#ifndef __roTypeCast_h__
#define __roTypeCast_h__

#include "roTypeOf.h"

inline void _roCastAssert(roUint64 from, roUint32)	{ roAssert(from <= ro::TypeOf<roUint32>::valueMax()); }
inline void _roCastAssert(roUint64 from, roUint16)	{ roAssert(from <= ro::TypeOf<roUint16>::valueMax()); }
inline void _roCastAssert(roUint64 from, roUint8)	{ roAssert(from <= ro::TypeOf<roUint8>::valueMax()); }
inline void _roCastAssert(roUint64 from, roInt32)	{ roAssert(from <= (roUint64)ro::TypeOf<roInt32>::valueMax()); }
inline void _roCastAssert(roUint64 from, roInt16)	{ roAssert(from <= (roUint64)ro::TypeOf<roInt16>::valueMax()); }
inline void _roCastAssert(roUint64 from, roInt8)	{ roAssert(from <= (roUint64)ro::TypeOf<roInt8>::valueMax()); }

inline void _roCastAssert(roUint32 from, roUint16)	{ roAssert(from <= ro::TypeOf<roUint16>::valueMax()); }
inline void _roCastAssert(roUint32 from, roUint8)	{ roAssert(from <= ro::TypeOf<roUint8>::valueMax()); }
inline void _roCastAssert(roUint32 from, roInt16)	{ roAssert(from <= (roUint32)ro::TypeOf<roInt16>::valueMax()); }
inline void _roCastAssert(roUint32 from, roInt8)	{ roAssert(from <= (roUint32)ro::TypeOf<roInt8>::valueMax()); }

inline void _roCastAssert(roUint16 from, roUint8)	{ roAssert(from <= ro::TypeOf<roUint8>::valueMax());}
inline void _roCastAssert(roUint16 from, roInt8)	{ roAssert(from <= (roUint16)ro::TypeOf<roInt8>::valueMax());}

template<class From, class To>
inline void _roCastAssert(From from, To)			{}

template<class Target, class Source>
inline Target num_cast(Source x)
{
	_roCastAssert(x, Target());
	return static_cast<Target>(x);
}

#endif	// __roTypeCast_h__
