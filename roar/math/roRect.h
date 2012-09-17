#ifndef __math_roRect_h__
#define __math_roRect_h__

namespace ro {

/// Represent a rectangle with with and height
struct Sizef
{
	Sizef() { w = h = 0; }
	Sizef(float w_, float h_) { w = w_; h = h_; }

	float w, h;
};

/// Rectangle, assume origin at top left corner
struct Rectf
{
	Rectf()											{ x = y = w = h = 0; }
	explicit Rectf(const float* p4f)				{ x = p4f[0]; x = p4f[1]; x = p4f[2]; x = p4f[3]; }
	explicit Rectf(const Sizef& size)				{ x = y = 0; w = size.w; h = size.h; }
	Rectf(float x_, float y_)						{ x = x_; y = y_; w = h = 0; }
	Rectf(float x_, float y_, const Sizef& size)	{ x = x_; y = y_; w = size.w; h = size.h; }
	Rectf(float x_, float y_, float w_, float h_)	{ x = x_; y = y_; w = w_; h = h_; }

// Operations
	bool isPointInRect(float px, float py) const	{ return px > left() && px < right() && py > top() && py < bottom(); }

// Attributes
	float left() const		{ return x; }
	float right() const		{ return x + w; }
	float top() const		{ return y; }
	float bottom() const	{ return y + h; }
	float centerx() const	{ return x + w / 2; }
	float centery() const	{ return y + h / 2; }

	Sizef size() const				{ return Sizef(w, h); }
	void setSize(const Sizef& size)	{ w = size.w; h = size.h; }

	union { struct {
		float x, y, w, h;
	};
		float data[4];
	};
};	// Rectf

}	// namespace ro

#endif	// __math_roRect_h__
