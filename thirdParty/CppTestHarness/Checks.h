#ifndef CPPTESTHARNESS_CHECKS_H
#define CPPTESTHARNESS_CHECKS_H

#include "Config.h"
#include <sstream>

namespace CppTestHarness
{
	template< typename Value >
	bool Check(Value const value)
	{
#ifdef VISUAL_STUDIO
#	pragma warning(push)
#	pragma warning(disable:4127) // conditional expression is constant
#	pragma warning(disable:4800) // forcing value to bool true/false, performance warning
#endif
		return value;
#ifdef VISUAL_STUDIO
#	pragma warning(pop)
#endif
	}

	template< typename Value >
	bool CheckNull(Value const value)
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

#ifdef VISUAL_STUDIO
	typedef __int8	int8_t;
	typedef __int16	int16_t;
	typedef __int32	int32_t;
	typedef __int64	int64_t;
	typedef unsigned __int8		uint8_t;
	typedef unsigned __int16	uint16_t;
	typedef unsigned __int32	uint32_t;
	typedef unsigned __int64	uint64_t;
#endif

	template<typename T>
	inline bool IsSigned(T t) { return false; }
	inline bool IsSigned(int8_t) { return true; }
	inline bool IsSigned(int16_t) { return true; }
	inline bool IsSigned(int32_t) { return true; }
	inline bool IsSigned(int64_t) { return true; }

	template<typename T> struct UnsignedCounterPart	{ typedef T Ret;		};
	template<>	struct UnsignedCounterPart<int8_t>	{ typedef uint8_t Ret;	};
	template<>	struct UnsignedCounterPart<int16_t>	{ typedef uint16_t Ret;	};
	template<>	struct UnsignedCounterPart<int32_t>	{ typedef uint32_t Ret;	};
	template<>	struct UnsignedCounterPart<int64_t>	{ typedef uint64_t Ret;	};

	template<typename T1, typename T2>
	bool IsEqual(T1 t1, T2 t2)
	{
		if(IsSigned(t1) == IsSigned(t2))
			return (T1)t1 == (T1)t2;
		if(t1 < 0 && t2 >= 0) return false;
		if(t2 < 0 && t1 >= 0) return false;
		UnsignedCounterPart<T1>::Ret t1_(t1);
		UnsignedCounterPart<T2>::Ret t2_(t2);
		return t1_ == t2_;
	}

	inline bool IsEqual(bool t1, bool t2)				{ return t1 == t2; }
	inline bool IsEqual(	  char* s1,		  char* s2)	{ return strcmp(s1, s2) == 0; }
	inline bool IsEqual(const char* s1, const char* s2)	{ return strcmp(s1, s2) == 0; }
	inline bool IsEqual(	  char* s1, const char* s2)	{ return strcmp(s1, s2) == 0; }
	inline bool IsEqual(const char* s1,		  char* s2)	{ return strcmp(s1, s2) == 0; }

	template< typename Actual, typename Expected >
	bool CheckEqual(Actual const actual, Expected const expected)
	{
#ifdef VISUAL_STUDIO
#	pragma warning(push)
#	pragma warning(disable:4127) // conditional expression is constant
#endif
		return IsEqual(actual, expected);
#ifdef VISUAL_STUDIO
#	pragma warning(pop)
#endif
	}

	template< typename Actual, typename Expected >
	bool CheckArrayEqual(Actual const actual, Expected const expected, int const count)
	{
		for (int i = 0; i < count; ++i)
		{
			if (!(actual[i] == expected[i]))
				return false;
		}

		return true;
	}

	template< typename Actual, typename Expected, typename Tolerance >
	bool CheckClose(Actual const actual, Expected const expected, Tolerance const tolerance)
	{
		double const diff = double(actual) - double(expected);
		double const doubleTolerance = double(tolerance);
		double const positiveDoubleTolerance = (doubleTolerance >= 0.0) ? doubleTolerance : -doubleTolerance;

		if (diff > positiveDoubleTolerance)
			return false;

		if (diff < -positiveDoubleTolerance)
			return false;

		return true;
	}

	template< typename Actual, typename Expected, typename Tolerance >
	bool CheckArrayClose(Actual const actual, Expected const expected, int const count, Tolerance const tolerance)
	{
		for (int i = 0; i < count; ++i)
		{
			if (!CheckClose(actual[i], expected[i], tolerance))
				return false;
		}

		return true;
	}

	std::string ToNarrowString(const wchar_t* wstr);

	inline std::string ToNarrowString(const std::wstring& wstr)
	{
		return ToNarrowString(wstr.c_str());
	}

	inline const char* ToNarrowString(const char* str)
	{
		return str;
	}

	inline const std::string& ToNarrowString(const std::string& str)
	{
		return str;
	}

	template< typename T >
	const T& ToNarrowString(const T& t)
	{
		return t;
	}

	template< typename Actual, typename Expected >
	std::string BuildFailureString(Actual const actual, Expected const expected)
	{
		std::stringstream failureStr;
		failureStr << "Expected " << ToNarrowString(expected) << " but got " << ToNarrowString(actual) << std::endl;
		return failureStr.str();
	}

	std::string BuildFailureString(const size_t actual, const size_t expected);

	template< typename Actual, typename Expected >
	std::string BuildFailureString(Actual const* actual, Expected const* expected, int const count)
	{
		std::stringstream failureStr;
		int i;

		failureStr << "Expected [ ";

		for (i = 0; i < count; ++i)
			failureStr << expected[i] << ' ';

		failureStr << "] but got [ ";

		for (i = 0; i < count; ++i)
			failureStr << actual[i] << ' ';

		failureStr << ']' << std::endl;

		return failureStr.str();
	}
}

#endif

