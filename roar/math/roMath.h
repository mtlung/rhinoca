#ifndef __math_roMath_h__
#define __math_roMath_h__

#include <math.h>

float roFAbs(float x);
float roSqrt(float x);
float roInvSqrt(float x);
float roInvSqrtFast(float x);
float roFloor(float x);

float roSin(float x);
float roCos(float x);
float roTan(float x);


// ----------------------------------------------------------------------

inline float roFAbs(float x) { return ::fabsf(x); }
inline float roSqrt(float x) { return ::sqrtf(x); }
inline float roInvSqrt(float x) { return 1.0f / ::sqrtf(x); }
inline float roInvSqrtFast(float x) { return 1.0f / ::sqrtf(x); }
inline float roFloor(float x) { return floorf(x); }

inline float roSin(float x) { return ::sinf(x); }
inline float roCos(float x) { return ::cosf(x); }
inline float roTan(float x) { return ::tanf(x); }

#endif	// __math_roMath_h__
