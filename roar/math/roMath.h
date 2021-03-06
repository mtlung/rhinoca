#ifndef __math_roMath_h__
#define __math_roMath_h__

#include "../base/roUtility.h"
#include <math.h>

constexpr float roPI			= 3.14159265358979323846f;	// pi
constexpr float roTWO_PI		= 2.0f * roPI;				// pi * 2
constexpr float roHALF_PI		= 0.5f * roPI;				// pi / 2
constexpr float roONEFOURTH_PI	= 0.25f * roPI;				// pi / 4
constexpr float roE				= 2.71828182845904523536f;	// e
constexpr float roSQRT_TWO		= 1.41421356237309504880f;	// sqrt( 2 )
constexpr float roSQRT_THREE	= 1.73205080756887729352f;	// sqrt( 3 )
constexpr float roSQRT_1OVER2	= 0.70710678118654752440f;	// sqrt( 1 / 2 )
constexpr float roSQRT_1OVER3	= 0.57735026918962576450f;	// sqrt( 1 / 3 )
constexpr float roM_DEG2RAD		= roPI / 180.0f;			// Degrees to radians multiplier
constexpr float roM_RAD2DEG		= 180.0f / roPI;			// Radians to degrees multiplier
constexpr float roM_SEC2MS		= 1000.0f;					// Seconds to milliseconds multiplier
constexpr float roM_MS2SEC		= 0.001f;					// Milliseconds to seconds multiplier
constexpr float roINFINITY		= 1e30f;					// Huge number which should be larger than any valid number used
constexpr float roFLT_EPSILON	= 1.192092896e-07f;			// Smallest positive number such that 1.0+FLT_EPSILON != 1.0

bool roIsNearZero			(float x, float epsilon = roFLT_EPSILON);
bool roIsNearEqual			(float a, float b, float epsilon = roFLT_EPSILON);

float roFAbs				(float x);
float roSqrt				(float x);
float roInvSqrt				(float x);
float roInvSqrtFast			(float x);

constexpr float roDeg2Rad	(float x);
constexpr float roRad2Deg	(float x);

float roSin					(float x);
float roCos					(float x);
float roTan					(float x);
void roSinCos				(float x, float& s, float& c);

float roFloor				(float x);
float roCeil				(float x);
float roFrac				(float x);
float roRound				(float x);	// < 0.5 floor, > 0.5 ceil
float roRoundTo				(float x, unsigned digit);

double roFloor				(double x);
double roCeil				(double x);
double roFrac				(double x);
double roRound				(double x);	// < 0.5 floor, > 0.5 ceil
double roRoundTo			(double x, unsigned digit);

double roPow10d				(int n);	// return 10.0^n

template<class T> T roGcd	(T a, T b);	// Greatest common divisor

template<class T> T roClamp	(T x, T min, T max);
template<class T> T roClampMin(T x, T min);
template<class T> T roClampMax(T x, T max);

template<class T> T roClampedSubtraction(T a, T b);

constexpr bool roIsSameSign	(float v1, float v2);
bool roIsPowerOfTwo			(unsigned x);
unsigned roNextPowerOfTwo	(unsigned x);

roUint64 roFactorial		(roUint64 n);

// A bunch of interpolation functions
// http://sol.gfxile.net/interpolation/index.html
template<class T> T roStepLinear(const T& v1, const T& v2, float t);
template<class T> T roStepSmooth(const T& v1, const T& v2, float t);
template<class T> T roStepRunAvg(const T& oldVal, const T& newVal, unsigned avgOverFrame);


// ----------------------------------------------------------------------

inline bool roIsNearZero(float x, float epsilon) { return roFAbs(x) < epsilon; }
inline bool roIsNearEqual(float a, float b, float epsilon) { return roIsNearZero(a - b, epsilon); }

inline float roFAbs(float x) { return ::fabsf(x); }
inline float roSqrt(float x) { return ::sqrtf(x); }
inline float roInvSqrt(float x) { return 1.0f / ::sqrtf(x); }
inline float roInvSqrtFast(float x) { return 1.0f / ::sqrtf(x); }

inline double roSqrt(double x) { return ::sqrt(x); }

constexpr inline float roDeg2Rad(float x) { return x * roM_DEG2RAD; }
constexpr inline float roRad2Deg(float x) { return x * roM_RAD2DEG; }

inline float roSin(float x) { return ::sinf(x); }
inline float roCos(float x) { return ::cosf(x); }
inline float roTan(float x) { return ::tanf(x); }

inline float roFloor(float x) { return ::floorf(x); }
inline float roCeil(float x) { return ::ceilf(x); }
inline float roFrac(float x) { return x - roFloor(x); }
inline float roRound(float x) { return ::floorf(x + 0.5f); }

inline double roFloor(double x) { return ::floor(x); }
inline double roCeil(double x) { return ::ceil(x); }
inline double roFrac(double x) { return x - roFloor(x); }
inline double roRound(double x) { return ::floor(x + 0.5); }

template<class T>
inline T roGcd(T a, T b)
{
	T r = a % b;
	if(r == 0) return b;
	return roGcd(b, r);
}

template<class T>
inline T roClampedSubtraction(T a, T b) { return a > b ? a - b : 0; }

template<class T>
inline T roStepLinear(const T& v1, const T& v2, float t) { return v1 + (v2 - v1) * t; }

template<class T>
inline T roStepSmooth(const T& v1, const T& v2, float t)
{
	t = t * t * (3 - 2 * t);
	return roStepLinear(v1, v2, t);
}

template<class T>
inline T roStepRunAvg(const T& oldVal, const T& newVal, unsigned avgOverFrame)
{
	roAssert(avgOverFrame > 0);
	return ((oldVal * (avgOverFrame - 1)) + newVal) / avgOverFrame;
}

inline constexpr bool roIsSameSign(float v1, float v2) { return (v1 > 0) == (v2 > 0); }

inline bool roIsPowerOfTwo(unsigned x) { return x == roNextPowerOfTwo(x); }

inline unsigned roNextPowerOfTwo(unsigned x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >>16);
	return x + 1;
}

inline roUint64 roFactorial(roUint64 n)
{
	if(n > 20) { roAssert("roFactorial overflow"); return 0; }
	roUint64 ret = 1;
	if(n > 0) for(roUint64 i=n; i>=1; --i)
		ret *= i;
	return ret;
}


#endif	// __math_roMath_h__
