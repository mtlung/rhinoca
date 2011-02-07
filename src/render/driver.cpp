#include "pch.h"
#include "driver.h"
#include "../mat44.h"

namespace Render {

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

}	// Render
