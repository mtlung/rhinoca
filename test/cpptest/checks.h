#ifndef __cpptest_checks_h__
#define __cpptest_checks_h__

#include "../../roar/base/roNonCopyable.h"
#include "../../roar/base/roString.h"
#include "../../roar/base/roStringFormat.h"
#include "../../roar/base/roTypeOf.h"

namespace ro {
namespace cpptest {

template<typename T>
bool check(const T value)
{
#ifdef VISUAL_STUDIO
#	pragma warning(push)
#	pragma warning(disable:4127) // conditional expression is constant
#	pragma warning(disable:4800) // forcing T to bool true/false, performance warning
#endif
	return value;
#ifdef VISUAL_STUDIO
#	pragma warning(pop)
#endif
}

template<typename T>
bool check(T* value)					{ return value != NULL; }
template<typename T>
bool check(const T* value)				{ return value != NULL; }
inline bool check(void* value)			{ return value != NULL; }
inline bool check(const void* value)	{ return value != NULL; }

template<typename T>
bool checkNull(const T value)
{
#ifdef VISUAL_STUDIO
#	pragma warning(push)
#	pragma warning(disable:4127) // conditional expression is constant
#endif
	return value == (void*)(0);
#ifdef VISUAL_STUDIO
#	pragma warning(pop)
#endif
}

struct TrueType {};
struct FalseType {};
template<typename T> struct IsIntType	{ typedef FalseType Ret;};
template<>	struct IsIntType<roInt8>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roInt16>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roInt32>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roInt64>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roUint8>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roUint16>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roUint32>	{ typedef TrueType Ret;	};
template<>	struct IsIntType<roUint64>	{ typedef TrueType Ret;	};

template<typename T1, typename T2>
bool IsEqualInteger(T1 t1, T2 t2)
{
	if(TypeOf<T1>::isSigned() == TypeOf<T2>::isSigned())
		return (T1)t1 == (T1)t2;
	if(t1 < 0 && t2 >= 0) return false;
	if(t2 < 0 && t1 >= 0) return false;
	UnsignedCounterPart<T1>::Ret t1_(t1);
	UnsignedCounterPart<T2>::Ret t2_(t2);
	return t1_ == t2_;
}

template<typename T1, typename T2>
bool IsEqual(T1 t1, T2 t2)
{
	return t1 == t2;
}

inline bool IsEqual(	  char* s1,		  char* s2)	{ return roStrCmp(s1, s2) == 0; }
inline bool IsEqual(const char* s1, const char* s2)	{ return roStrCmp(s1, s2) == 0; }
inline bool IsEqual(	  char* s1, const char* s2)	{ return roStrCmp(s1, s2) == 0; }
inline bool IsEqual(const char* s1,		  char* s2)	{ return roStrCmp(s1, s2) == 0; }

template<typename T1, typename T2, typename TF1, typename TF2>
bool IsEqual_(T1 t1, T2 t2, TF1, TF2)
{
	return IsEqual(t1, t2);
}

template<typename T1, typename T2>
bool IsEqual_(T1 t1, T2 t2, TrueType, TrueType)
{
	return IsEqualInteger(t1, t2);
}

template<typename Actual, typename Expected>
bool checkEqual(const Actual actual, const Expected expected)
{
#ifdef VISUAL_STUDIO
#	pragma warning(push)
#	pragma warning(disable:4127) // conditional expression is constant
#endif
	return IsEqual_(actual, expected, IsIntType<Actual>::Ret(), IsIntType<Expected>::Ret());
#ifdef VISUAL_STUDIO
#	pragma warning(pop)
#endif
}

template<typename Actual, typename Expected>
bool checkArrayEqual(Actual const actual, Expected const expected, roSize count)
{
	for (roSize i=0; i<count; ++i)
		if (!(actual[i] == expected[i]))
			return false;

	return true;
}

template<typename Actual, typename Expected, typename Tolerance>
bool checkClose(Actual const actual, Expected const expected, Tolerance const tolerance)
{
	double const diff = double(actual) - double(expected);
	double const doubleTolerance = double(tolerance);
	double const positiveDoubleTolerance = (doubleTolerance >= 0.0) ? doubleTolerance : -doubleTolerance;

	if(diff > positiveDoubleTolerance)
		return false;

	if(diff < -positiveDoubleTolerance)
		return false;

	return true;
}

template<typename Actual, typename Expected, typename Tolerance>
bool checkArrayClose(Actual const actual, Expected const expected, roSize count, Tolerance const tolerance)
{
	for(roSize i=0; i<count; ++i)
		if(!CheckClose(actual[i], expected[i], tolerance))
			return false;

	return true;
}

template<typename Actual, typename Expected>
String buildFailureString(const Actual& actual, const Expected& expected)
{
	String str;
	roVerify(strFormat(str, "Expected {} but got {}", expected, actual));
	return str;
}

template<typename Actual, typename Expected>
String buildFailureString(Actual* actual, Expected* expected)
{
	String str;
	roVerify(strFormat(str, "Expected 0x{} but got 0x{}", expected, actual));
	return str;
}

template<typename Actual>
String buildFailureString(const Actual& actual, const roStatus& expected)
{
	String str;
	roStatus stActual(actual);
	roVerify(strFormat(str, "Expected {} but got {}", expected.c_str(), stActual.c_str()));
	return str;
}

template<typename Expected>
String buildFailureString(const roStatus& actual, const Expected& expected)
{
	String str;
	roStatus stExpected(expected);
	roVerify(strFormat(str, "Expected {} but got {}", stExpected.c_str(), actual.c_str()));
	return str;
}

String buildFailureString(size_t actual, size_t expected);

template<typename Actual, typename Expected>
String buildFailureString(const Actual* actual, const Expected* expected, roSize count)
{
	String str = "Expected [ ";

	for(roSize i = 0; i<count; ++i)
		roVerify(strFormat(str, "{} ", expected[i]));

	roVerify(strFormat(str, "] but got [ "));

	for(roSize i = 0; i<count; ++i)
		roVerify(strFormat(str, "{} ", actual[i]));

	roVerify(strFormat(str, "]\n"));

	return str;
}

}	// namespace cpptest
}	// namespace ro

#endif	// __cpptest_checks_h__
