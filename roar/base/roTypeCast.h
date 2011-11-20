#ifndef __roTypeCast_h__
#define __roTypeCast_h__

#include "roTypeOf.h"

void _roCastAssert(roUint32 from, roUint16) {
	roAssert(from <= ro::TypeOf<roUint16>::valueMax());
}

template<class Target, class Source>
Target num_cast(Source x)
{
	_roCastAssert(x, Target());
	return static_cast<Target>(x);
}

#endif	// __roTypeCast_h__
