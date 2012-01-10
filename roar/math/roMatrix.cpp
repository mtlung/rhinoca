#include "pch.h"
#include "roMatrix.h"
#include "../platform/roCompiler.h"
#include "../platform/roOS.h"

#if roCPU_SSE
#	include <xmmintrin.h>
#endif

namespace ro {

const Mat4 mat4Identity(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
);

Mat4 makeScaleMat4(const float scale[3])
{
	Mat4 ret(mat4Identity);
	ret.m00 = scale[0];
	ret.m11 = scale[1];
	ret.m22 = scale[2];
	return ret;
}

Mat4 makeAxisRotationMat4(const float _axis[3], float angle)
{
	// Reference: OgreMatrix3.cpp
	float c, s;
	c = cosf(angle);
	s = sinf(angle);

	const Vec3& axis = *reinterpret_cast<const Vec3*>(_axis);
	const float oneMinusCos = 1 - c;
	const float x2 = axis.x * axis.x;
	const float y2 = axis.y * axis.y;
	const float z2 = axis.z * axis.z;
	const float xym = axis.x * axis.y * oneMinusCos;
	const float xzm = axis.x * axis.z * oneMinusCos;
	const float yzm = axis.y * axis.z * oneMinusCos;
	const float xSin = axis.x * s;
	const float ySin = axis.y * s;
	const float zSin = axis.z * s;

	Mat4 ret(mat4Identity);
	ret.m00 = x2 * oneMinusCos + c;
	ret.m01 = xym - zSin;
	ret.m02 = xzm + ySin;
	ret.m10 = xym + zSin;
	ret.m11 = y2 * oneMinusCos + c;
	ret.m12 = yzm - xSin;
	ret.m20 = xzm - ySin;
	ret.m21 = yzm + xSin;
	ret.m22 = z2 * oneMinusCos + c;
	return ret;
}

Mat4 makeTranslationMat4(const float translation[3])
{
	Mat4 ret(mat4Identity);
	ret.m03 = translation[0];
	ret.m13 = translation[1];
	ret.m23 = translation[2];
	return ret;
}

void mat4MulVec3(const float m[4][4], const float src[3], float dst[3])
{
	float x = src[0];
	float y = src[1];
	float z = src[2];

	float s = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
	if(s == 0.0f) {
		dst[0] = dst[1] = dst[2] = 0;
	}
	else if(s == 1.0f) {
		dst[0] = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
		dst[1] = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
		dst[2] = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
	}
	else {
		float invS = 1.0f / s;
		dst[0] = (m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0]) * invS;
		dst[1] = (m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1]) * invS;
		dst[2] = (m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]) * invS;
	}
}

void mat4MulVec4(const float m[4][4], const float src[4], float dst[4])
{
#if roCPU_SSE
	__m128 x0 = _mm_loadu_ps(m[0]);
	__m128 x1 = _mm_loadu_ps(m[1]);
	__m128 x2 = _mm_loadu_ps(m[2]);
	__m128 x3 = _mm_loadu_ps(m[3]);

	__m128 v = _mm_loadu_ps(src);
	__m128 v0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0,0,0,0));
	__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1,1,1,1));
	__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2,2,2,2));
	__m128 v3 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3,3,3,3));

	v0 = _mm_mul_ps(x0, v0);
	v1 = _mm_mul_ps(x1, v1);
	v2 = _mm_mul_ps(x2, v2);
	v3 = _mm_mul_ps(x3, v3);

	x0 = _mm_add_ps(v0, v1);
	x1 = _mm_add_ps(v2, v3);
	v = _mm_add_ps(x0, x1);

	_mm_storeu_ps(dst, v);
#else
	// Local variables to prevent parameter aliasing
	const float x = src[0];
	const float y = src[1];
	const float z = src[2];
	const float w = src[3];

	dst[0] = m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3] * w;
	dst[1] = m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3] * w;
	dst[2] = m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3] * w;
	dst[3] = m[3][0] * x + m[3][1] * y + m[3][2] * z + m[3][3] * w;
#endif
}

void mat4MulMat4(const float lhs[4][4], const float rhs[4][4], float dst[4][4])
{
	roAssert(lhs != rhs);
	roAssert(lhs != dst);

#if roCPU_SSE
	// Reference for SSE matrix multiplication:
	// http://www.cortstratton.org/articles/OptimizingForSSE.php
	__m128 x4 = _mm_loadu_ps(lhs[0]);
	__m128 x5 = _mm_loadu_ps(lhs[1]);
	__m128 x6 = _mm_loadu_ps(lhs[2]);
	__m128 x7 = _mm_loadu_ps(lhs[3]);

	__m128 x0, x1, x2, x3;

	for(unsigned i=0; i<4; ++i) {
		x1 = x2 = x3 = x0 = _mm_loadu_ps(rhs[i]);
		x0 = _mm_shuffle_ps(x0, x0, _MM_SHUFFLE(0,0,0,0));
		x1 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(1,1,1,1));
		x2 = _mm_shuffle_ps(x2, x2, _MM_SHUFFLE(2,2,2,2));
		x3 = _mm_shuffle_ps(x3, x3, _MM_SHUFFLE(3,3,3,3));

		x0 = _mm_mul_ps(x0, x4);
		x1 = _mm_mul_ps(x1, x5);
		x2 = _mm_mul_ps(x2, x6);
		x3 = _mm_mul_ps(x3, x7);

		x2 = _mm_add_ps(x2, x0);
		x3 = _mm_add_ps(x3, x1);
		x3 = _mm_add_ps(x3, x2);

		_mm_storeu_ps(dst[i], x3);
	}
#else
	const float* m1Ptr = reinterpret_cast<const float*>(lhs);
	const float* m2Ptr = reinterpret_cast<const float*>(rhs);
	float* dstPtr = reinterpret_cast<float*>(dst);

	for(int i=0; i<4; ++i) {
		for(int j=0; j<4; ++j) {
			*dstPtr
			= m1Ptr[0 * 4 + j] * m2Ptr[0]
			+ m1Ptr[1 * 4 + j] * m2Ptr[1]
			+ m1Ptr[2 * 4 + j] * m2Ptr[2]
			+ m1Ptr[3 * 4 + j] * m2Ptr[3];
			++dstPtr;
		}
		m2Ptr += 4;
	}
#endif
}

Mat4 Mat4::transpose() const
{
	Mat4 transpose;

	for(roSize i=0; i<4; ++i) {
		for(roSize j=0; j<4; ++j) {
			transpose[ i ][ j ] = mat[ j ][ i ];
		}
	}
	return transpose;
}

Mat4& Mat4::transposeSelf()
{
	float temp;

	for(roSize i=0; i<4; ++i) {
		for(roSize j=0; j<4; ++j) {
			temp = mat[i][j];
			mat[i][j] = mat[j][i];
			mat[j][i] = temp;
		}
	}
	return *this;
}

Mat4 makeOrthoMat4(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Mat4 m(mat4Identity);

	m.m00 = 2.0f / (right - left);
	m.m03 = -(right + left) / (right - left);
	m.m11 = 2.0f / (top - bottom);
	m.m13 = -(top + bottom) / (top - bottom);
	m.m22 = -2.0f / (zFar - zNear);
	m.m23 = -(zFar + zNear) / (zFar - zNear);

	return m;
}

// See http://graphics.ucsd.edu/courses/cse167_f05/CSE167_04.ppt
Mat4 makePrespectiveMat4(float fovyDeg, float acpectWidthOverHeight, float zNear, float zFar)
{
	float f = 1.0f / tanf(roDeg2Rad(fovyDeg) * 0.5f);
	float nf = 1.0f / (zNear - zFar);

	Mat4 m;
	m.zero();

	m.m00 = f / acpectWidthOverHeight;	// One over width
	m.m11 = f;							// One over height
	m.m22 = (zFar + zNear) * nf;
	m.m23 = (2 * zFar * zNear) * nf;
	m.m32 = -1;							// Perspective division

	return m;
}

}	// namespace ro
