#ifndef __roTypeCast_h__
#define __roTypeCast_h__

#include "roTypeOf.h"
#include "roStatus.h"

// ----------------------------------------------------------------------

template<class Dst, class Src>
inline roStatus roIsValidCast(Src src)
{
	Dst tmp = static_cast<Dst>(src);

	if(ro::TypeOf<Src>::isSigned() && ro::TypeOf<Dst>::isUnsigned()) {
		// Signed to unsigned
		if(src < 0) return roStatus::numeric_cast_overflow;
	}
	else if(ro::TypeOf<Src>::isUnsigned() && ro::TypeOf<Dst>::isSigned()) {
		// Unsigned to signed
		if(tmp < 0) return roStatus::numeric_cast_overflow;
	}

	return (static_cast<Src>(tmp) == src) ? roStatus::ok : roStatus::numeric_cast_overflow;
}

template<>
inline roStatus roIsValidCast<double>(float src)
{
	return roStatus::ok;
}

template<>
inline roStatus roIsValidCast<float>(double src)
{
	float f = (float)src;
	double diff = src - f;
	const double epsilon = 1e-7;
	return (diff > -epsilon && diff < epsilon) ? roStatus::ok : roStatus::numeric_cast_overflow;
}

template<class Dst, class Src>
inline roStatus roIsValidCast(Dst dst, Src src)
{
	return roIsValidCast<Dst>(src);
}

template<class T>
inline roStatus roSafeAssign(T& dst, T src)
{
	dst = src;
	return roStatus::ok;
}

template<class Dst, class Src>
inline roStatus roSafeAssign(Dst& dst, Src src)
{
	roStatus st = roIsValidCast(dst, src);
	if(!st) return st;
	dst = static_cast<Dst>(src);
	return roStatus::ok;
}

/// Trigger assertion on invalid cast
template<class Dst, class Src>
inline Dst num_cast(Src x)
{
	roAssert(roIsValidCast<Dst>(x));
	return static_cast<Dst>(x);
}

/// Do truncation to the target type
template<class Dst, class Src>
inline Dst clamp_cast(Src src)
{
	if(ro::TypeOf<Src>::isSigned() && ro::TypeOf<Dst>::isUnsigned()) {
		// Signed to unsigned
		if(src < 0) return 0;
	}
	else if(ro::TypeOf<Src>::isUnsigned() && ro::TypeOf<Dst>::isSigned()) {
		// Unsigned to signed
		if(static_cast<Dst>(src) < 0) return ro::TypeOf<Dst>::valueMax();
	}

	return static_cast<Dst>(src);
}

/// This cast treat the source as a block of memory
template<class Dst, class Src>
inline Dst mem_cast(Src x)
{
	roStaticAssert(sizeof(Dst) == sizeof(Src));
	return *(Dst*)(&x);
}


#endif	// __roTypeCast_h__
