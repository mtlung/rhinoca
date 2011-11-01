#include "pch.h"
#include "rgutility.h"
#include "../mat44.h"
#include <math.h>
#include <string.h>	// for memset

static const float PI = 3.14159265358979f;

void rgMat44MakeIdentity(float outMat[16])
{
	Mat44& out = *reinterpret_cast<Mat44*>(outMat);
	out = Mat44::identity;
}

// See http://graphics.ucsd.edu/courses/cse167_f05/CSE167_04.ppt
void rgMat44MakePrespective(float outMat[16], float fovyDeg, float acpectWidthOverHeight, float znear, float zfar)
{
	float f = 1.0f / tanf((fovyDeg * PI / 180) * 0.5f);
	float nf = 1.0f / (znear - zfar);

	Mat44& out = *reinterpret_cast<Mat44*>(outMat);

	memset(outMat, 0, sizeof(Mat44));

	out.m00 = f / acpectWidthOverHeight;	// One over width
	out.m11 = f;							// One over height
	out.m22 = (zfar + znear) * nf;
	out.m23 = (2 * zfar * znear) * nf;
	out.m32 = -1;							// Prespective division
}

void rgMat44MulVec4(const float mat[16], const float in[4], float out[4])
{
	// Local variables to prevent parameter aliasing
	const float x = in[0];
	const float y = in[1];
	const float z = in[2];
	const float w = in[3];

	const Mat44& m = *reinterpret_cast<const Mat44*>(mat);

	out[0] = m.m00 * x + m.m01 * y + m.m02 * z + m.m03 * w;
	out[1] = m.m10 * x + m.m11 * y + m.m12 * z + m.m13 * w;
	out[2] = m.m20 * x + m.m21 * y + m.m22 * z + m.m23 * w;
	out[3] = m.m30 * x + m.m31 * y + m.m32 * z + m.m33 * w;
}

void rgMat44TranslateBy(float outMat[16], const float xyz[3])
{
	Mat44& out = *reinterpret_cast<Mat44*>(outMat);
	out.m03 += xyz[0];
	out.m13 += xyz[1];
	out.m23 += xyz[2];
}

static void swap(float& a, float& b)
{
	float tmp = a;
	a = b;
	b = tmp;
}

void rgMat44Transpose(float mat[16])
{
	Mat44& out = *reinterpret_cast<Mat44*>(mat);
	swap(out.m01, out.m10);
	swap(out.m02, out.m20);
	swap(out.m03, out.m30);
	swap(out.m12, out.m21);
	swap(out.m13, out.m31);
	swap(out.m23, out.m32);
}
