#ifndef __math_roMath_h__
#define __math_roMath_h__

#include <math.h>

static const float roPI				= 3.14159265358979323846f;	// pi
static const float roTWO_PI			= 2.0f * roPI;				// pi * 2
static const float roHALF_PI		= 0.5f * roPI;				// pi / 2
static const float roONEFOURTH_PI	= 0.25f * roPI;				// pi / 4
static const float roE				= 2.71828182845904523536f;	// e
static const float roSQRT_TWO		= 1.41421356237309504880f;	// sqrt( 2 )
static const float roSQRT_THREE		= 1.73205080756887729352f;	// sqrt( 3 )
static const float roSQRT_1OVER2	= 0.70710678118654752440f;	// sqrt( 1 / 2 )
static const float roSQRT_1OVER3	= 0.57735026918962576450f;	// sqrt( 1 / 3 )
static const float roM_DEG2RAD		= roPI / 180.0f;			// Degrees to radians multiplier
static const float roM_RAD2DEG		= 180.0f / roPI;			// Radians to degrees multiplier
static const float roM_SEC2MS		= 1000.0f;					// Seconds to milliseconds multiplier
static const float roM_MS2SEC		= 0.001f;					// Milliseconds to seconds multiplier
static const float roINFINITY		= 1e30f;					// Huge number which should be larger than any valid number used
static const float roFLT_EPSILON	= 1.192092896e-07f;			// Smallest positive number such that 1.0+FLT_EPSILON != 1.0

float roFAbs(float x);
float roSqrt(float x);
float roInvSqrt(float x);
float roInvSqrtFast(float x);
float roFloor(float x);

float roDeg2Rad(float x);
float roRad2Deg(float x);

float roSin(float x);
float roCos(float x);
float roTan(float x);

bool roIsPowerOfTwo(unsigned x);
unsigned roNextPowerOfTwo(unsigned x);


// ----------------------------------------------------------------------

inline float roFAbs(float x) { return ::fabsf(x); }
inline float roSqrt(float x) { return ::sqrtf(x); }
inline float roInvSqrt(float x) { return 1.0f / ::sqrtf(x); }
inline float roInvSqrtFast(float x) { return 1.0f / ::sqrtf(x); }
inline float roFloor(float x) { return floorf(x); }

inline float roDeg2Rad(float x) { return x * roM_DEG2RAD; }
inline float roRad2Deg(float x) { return x * roM_RAD2DEG; }

inline float roSin(float x) { return ::sinf(x); }
inline float roCos(float x) { return ::cosf(x); }
inline float roTan(float x) { return ::tanf(x); }

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


#endif	// __math_roMath_h__
