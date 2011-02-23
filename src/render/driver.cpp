#include "pch.h"
#include "driver.h"
#include "../mat44.h"

namespace Render {

unsigned Driver::nextPowerOfTwo(unsigned x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >>16);
	return x + 1;
}

void Driver::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Mat44 m = Mat44::identity;

	m.m00 = 2.0f / (right - left);
	m.m03 = -(right + left) / (right - left);
	m.m11 = 2.0f / (top - bottom);
	m.m13 = -(top + bottom) / (top - bottom);
	m.m22 = -2.0f / (zFar - zNear);
	m.m23 = -(zFar + zNear) / (zFar - zNear);
	m.m33 = 1;

	Driver::setProjectionMatrix(m.data);
}

void Driver::ortho(unsigned left, unsigned right, unsigned bottom, unsigned top, unsigned zNear, unsigned zFar)
{
	ortho((float)left, (float)right, (float)bottom, (float)top, (float)zNear, (float)zFar);
}

void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	float r, float g, float b, float a
)
{
	Driver::drawQuad(
		x1, y1, x2, y2,
		x3, y3, x4, y4,
		z,
		(rhuint8)(r * 255), (rhuint8)(g * 255), (rhuint8)(b * 255), (rhuint8)(a * 255)
	);
}

}	// Render
