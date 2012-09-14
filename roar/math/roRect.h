#ifndef __math_roRect_h__
#define __math_roRect_h__

#include "roVector.h"
#include "../base/roUtility.h"

namespace ro {

/// Rectangle, assume origin at top left corner
struct Rectf
{
	Rectf() { x = y = w = h = 0; }
	explicit Rectf(const float* p4f) { x = p4f[0]; x = p4f[1]; x = p4f[2]; x = p4f[3]; }
	Rectf(float x, float y) x(x), y(y) { w = h = 0; }
	Rectf(float x, float y, float w, float h) x(x), y(y), w(w), h(h) {}

	float left() const		{ return x; }
	float right() const		{ return x + w; }
	float top() const		{ return y; }
	float bottom() const	{ return y + h; }

	union {
		struct { float
			x, y, w, h;
		};
		float data[4];
	};
};	// Rectf

void rectTransformBound(float rect[4], const float mat[4][4]);

}	// namespace ro

#endif	// __math_roRect_h__
