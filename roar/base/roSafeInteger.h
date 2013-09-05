#ifndef __roSafeInteger_h__
#define __roSafeInteger_h__

#include "roStatus.h"
#include "roTypeOf.h"

#if roCPU_SSE
#	include <intrin.h>
#endif

// Reference:
// http://www.fefe.de/intof.html

template<typename T> struct _HigherIntType	{};
template<> struct _HigherIntType<roInt8>	{ typedef int ret;		typedef int forAddSub; };
template<> struct _HigherIntType<roUint8>	{ typedef unsigned ret;	typedef int forAddSub; };
template<> struct _HigherIntType<roInt16>	{ typedef int ret;		typedef int forAddSub; };
template<> struct _HigherIntType<roUint16>	{ typedef unsigned ret;	typedef int forAddSub; };
template<> struct _HigherIntType<roInt32>	{ typedef roInt64 ret;	typedef roInt64 forAddSub; };
template<> struct _HigherIntType<roUint32>	{ typedef roUint64 ret;	typedef roInt64 forAddSub; };

template<typename T>
roStatus roSafeAdd(T a, T b, T& ret)
{
	typedef _HigherIntType<T>::forAddSub Type;
	Type a_ = a, b_ = b;
	Type val = a_ + b_;
	ret = T(val);
	return T(val) == val ? roStatus::ok : roStatus::arithmetic_overflow;
}

template<typename T>
roStatus roSafeSub(T a, T b, T& ret)
{
	typedef _HigherIntType<T>::forAddSub Type;
	Type a_ = a, b_ = b;
	Type val = a_ - b_;
	ret = T(val);
	return T(val) == val ? roStatus::ok : roStatus::arithmetic_overflow;
}

template<typename T>
roStatus roSafeMul(T a, T b, T& ret)
{
	typedef _HigherIntType<T>::ret Type;
	Type a_ = a, b_ = b;
	Type val = a_ * b_;
	ret = T(val);
	return T(val) == val ? roStatus::ok : roStatus::arithmetic_overflow;
}

template<typename T>
roStatus roSafeDiv(T a, T b, T& ret)
{
	if(b == 0) return roStatus::division_with_zero;
	if(b == -1 && a < 0 && a == ro::TypeOf<T>::valueMin()) return roStatus::arithmetic_overflow;
	ret = a / b;
	return roStatus::ok;
}

template<typename T>
roStatus roSafeAbs(T a, T& ret)
{
	if(a < 0 && a == ro::TypeOf<T>::valueMin()) return roStatus::arithmetic_overflow;
	ret = (a < 0) ? (~a + 1) : a;
	return roStatus::ok;
}

template<typename T>
roStatus roSafeNeg(T a, T& ret)
{
	if(ro::TypeOf<T>::isUnsigned()) return roStatus::arithmetic_overflow;
	if(a < 0 && a == ro::TypeOf<T>::valueMin()) return roStatus::arithmetic_overflow;
	ret = -a;
	return roStatus::ok;
}

inline roStatus roSafeAdd(roUint64 a, roUint64 b, roUint64& ret)
{
	if(a > (ro::TypeOf<roUint64>::valueMax() - b))
		return roStatus::arithmetic_overflow;
	ret = a + b;
	return roStatus::ok;
}

inline roStatus roSafeAdd(roInt64 a, roInt64 b, roInt64& ret)
{
	if( (a > 0 && a > (ro::TypeOf<roInt64>::valueMax() - b)) ||
		(a < 0 && a < (ro::TypeOf<roInt64>::valueMin() - b)) )
		return roStatus::arithmetic_overflow;
	ret = a + b;
	return roStatus::ok;
}

inline roStatus roSafeSub(roUint64 a, roUint64 b, roUint64& ret)
{
	if(b > a) return roStatus::arithmetic_overflow;
	ret = a - b;
	return roStatus::ok;
}

inline roStatus roSafeSub(roInt64 a, roInt64 b, roInt64& ret)
{
	if( (a > 0 && a > (ro::TypeOf<roInt64>::valueMax() + b)) ||
		(a < 0 && a < (ro::TypeOf<roInt64>::valueMin() + b)) )
		return roStatus::arithmetic_overflow;
	ret = a - b;
	return roStatus::ok;
}

inline roStatus roSafeMul(roUint64 a, roUint64 b, roUint64& ret)
{
	// Consider that a*b can be broken up into:
	// (aHigh * 2^32 + aLow) * (bHigh * 2^32 + bLow)
	// => (aHigh * bHigh * 2^64) + (aLow * bHigh * 2^32) + (aHigh * bLow * 2^32) + (aLow * bLow)
	// TODO: May use _umul128 intrinsic
	roUint32 aHigh = (roUint32)(a >> 32);
	roUint32 aLow  = (roUint32)a;
	roUint32 bHigh = (roUint32)(b >> 32);
	roUint32 bLow  = (roUint32)b;

	if(aHigh * bHigh > 0)
		return roStatus::arithmetic_overflow;

	roUint64 tmp = (roUint64)aLow * (roUint64)bHigh + (roUint64)aHigh * (roUint64)bLow;
	if((tmp >> 32) != 0)
		return roStatus::arithmetic_overflow;

	roUint64 tmp2 = (roUint64)aLow * (roUint64)bLow;
	tmp = (tmp << 32) + tmp2;

	if(tmp < tmp2)
		return roStatus::arithmetic_overflow;

	ret = tmp;
	return roStatus::ok;
}

inline roStatus roSafeMul(roInt64 a, roInt64 b, roInt64& ret)
{
	typedef roInt64 inType;
	typedef roUint64 tmpType;

	tmpType absa = (a < 0) ? -a : a;
	tmpType absb = (b < 0) ? -b : b;
	tmpType absRet;

	roStatus st = roSafeMul(absa, absb, absRet);
	if(!st) return st;

	// Negative result expected
	if((a ^ b) < 0) {
		if(absRet > ((tmpType)ro::TypeOf<inType>::valueMax() + 1))
			return roStatus::arithmetic_overflow;
		ret = -inType(absRet);
	}
	// Result should be positive
	else {
		if(absRet > (tmpType)ro::TypeOf<inType>::valueMax())
			return roStatus::arithmetic_overflow;
		ret = inType(absRet);
	}

	return roStatus::ok;
}

template<typename T>
T roAssertAdd(T a, T b)
{
#if roDEBUG
	T ret;
	roStatus st = roSafeAdd(a, b, ret);
	roAssert(st == roStatus::ok);
#endif
	return a + b;
}

template<typename T>
T roAssertSub(T a, T b)
{
#if roDEBUG
	T ret;
	roStatus st = roSafeSub(a, b, ret);
	roAssert(st == roStatus::ok);
#endif
	return a - b;
}

template<typename T>
T roAssertMul(T a, T b)
{
#if roDEBUG
	T ret;
	roStatus st = roSafeMul(a, b, ret);
	roAssert(st == roStatus::ok);
#endif
	return a * b;
}

template<typename T>
T roAssertDiv(T a, T b)
{
#if roDEBUG
	T ret;
	roStatus st = roSafeDiv(a, b, ret);
	roAssert(st == roStatus::ok);
#endif
	return a / b;
}

template<typename T>
T roAssertAbs(T a)
{
	roAssert(a >= 0 || a != ro::TypeOf<T>::valueMin());
	return (a < 0) ? (~a + 1) : a;
}

template<typename T>
T roAssertNeg(T a)
{
	roAssert(!ro::TypeOf<T>::isUnsigned());
	roAssert(a >= 0 || a != ro::TypeOf<T>::valueMin());
	return -a;
}

template<typename T1, typename T2>
bool roIsGreater(T1 t1, T2 t2)
{
	if(ro::TypeOf<T1>::isSigned() == ro::TypeOf<T2>::isSigned())
		return (T1)t1 > (T1)t2;

	if(t1 < 0) return false;
	if(t2 < 0) return true;

	ro::UnsignedCounterPart<T1>::Ret t1_(t1);
	ro::UnsignedCounterPart<T2>::Ret t2_(t2);
	return t1_ > t2_;
}

template<typename T1, typename T2>
bool roIsLess(T1 t1, T2 t2)
{
	return roIsGreater(t2, t1);
}

#endif	// __roSafeInteger_h__
