#include "pch.h"
#include "mat44.h"
#include "common.h"
#include <math.h>

#if !defined(RHINOCA_GCC) || defined(__SSE__)
#   include <xmmintrin.h>
#endif

#if defined(RHINOCA_IOS_DEVICE)
//#	include "vfpmath/matrix_impl.h"
#endif

#include <memory.h> // For memcpy

struct _Vec3 {
	union {
		struct { float x, y, z; };
		float data[3];
	};
};

void Mat44::copyFrom(const float* p) {
	::memcpy(data, p, sizeof(float) * 16);
}

void Mat44::copyTo(float* p) const {
	::memcpy(p, data, sizeof(float) * 16);
}

void Mat44::mul(const Mat44& rhs, Mat44& ret) const
{
	ASSERT(&rhs != &ret);
	ASSERT(this != &ret);

#if 0 && defined(RHINOCA_IOS_DEVICE)

	Matrix4Mul(this->getPtr(), rhs.getPtr(), ret.getPtr());

#elif !defined(RHINOCA_GCC) || defined(__SSE__)

	// Reference for SSE matrix multiplication:
	// http://www.cortstratton.org/articles/OptimizingForSSE.php
	__m128 x4 = _mm_loadu_ps(c0);
	__m128 x5 = _mm_loadu_ps(c1);
	__m128 x6 = _mm_loadu_ps(c2);
	__m128 x7 = _mm_loadu_ps(c3);

	__m128 x0, x1, x2, x3;

	for(unsigned i=0; i<4; ++i) {
		x1 = x2 = x3 = x0 = _mm_loadu_ps(rhs.data2D[i]);
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

		_mm_storeu_ps(ret.data2D[i], x3);
	}

#else

	float a0, a1, a2, a3;

	a0 = m00; a1 = m01; a2 = m02; a3 = m03;
	ret.m00 = a0 * rhs.m00 + a1 * rhs.m10 + a2 * rhs.m20 + a3 * rhs.m30;
	ret.m01 = a0 * rhs.m01 + a1 * rhs.m11 + a2 * rhs.m21 + a3 * rhs.m31;
	ret.m02 = a0 * rhs.m02 + a1 * rhs.m12 + a2 * rhs.m22 + a3 * rhs.m32;
	ret.m03 = a0 * rhs.m03 + a1 * rhs.m13 + a2 * rhs.m23 + a3 * rhs.m33;

	a0 = m10; a1 = m11; a2 = m12; a3 = m13;
	ret.m10 = a0 * rhs.m00 + a1 * rhs.m10 + a2 * rhs.m20 + a3 * rhs.m30;
	ret.m11 = a0 * rhs.m01 + a1 * rhs.m11 + a2 * rhs.m21 + a3 * rhs.m31;
	ret.m12 = a0 * rhs.m02 + a1 * rhs.m12 + a2 * rhs.m22 + a3 * rhs.m32;
	ret.m13 = a0 * rhs.m03 + a1 * rhs.m13 + a2 * rhs.m23 + a3 * rhs.m33;

	a0 = m20; a1 = m21; a2 = m22; a3 = m23;
	ret.m20 = a0 * rhs.m00 + a1 * rhs.m10 + a2 * rhs.m20 + a3 * rhs.m30;
	ret.m21 = a0 * rhs.m01 + a1 * rhs.m11 + a2 * rhs.m21 + a3 * rhs.m31;
	ret.m22 = a0 * rhs.m02 + a1 * rhs.m12 + a2 * rhs.m22 + a3 * rhs.m32;
	ret.m23 = a0 * rhs.m03 + a1 * rhs.m13 + a2 * rhs.m23 + a3 * rhs.m33;

	a0 = m30; a1 = m31; a2 = m32; a3 = m33;
	ret.m30 = a0 * rhs.m00 + a1 * rhs.m10 + a2 * rhs.m20 + a3 * rhs.m30;
	ret.m31 = a0 * rhs.m01 + a1 * rhs.m11 + a2 * rhs.m21 + a3 * rhs.m31;
	ret.m32 = a0 * rhs.m02 + a1 * rhs.m12 + a2 * rhs.m22 + a3 * rhs.m32;
	ret.m33 = a0 * rhs.m03 + a1 * rhs.m13 + a2 * rhs.m23 + a3 * rhs.m33;

#endif
}

Mat44 Mat44::operator*(const Mat44& rhs) const
{
	Mat44 ret;
	mul(rhs, ret);
	return ret;
}

Mat44& Mat44::operator*=(const Mat44& rhs)
{
	*this = *this * rhs;
	return *this;
}

Mat44& Mat44::operator*=(float rhs)
{
	for(unsigned i=0; i<16; ++i)
		data[i] *= rhs;
	return *this;
}

void Mat44::transpose(Mat44& ret) const
{
	ASSERT(&ret != this);

	ret.m00 = m00; ret.m01 = m10; ret.m02 = m20; ret.m03 = m30;
	ret.m10 = m01; ret.m11 = m11; ret.m12 = m21; ret.m13 = m31;
	ret.m20 = m02; ret.m21 = m12; ret.m22 = m22; ret.m23 = m32;
	ret.m30 = m03; ret.m31 = m13; ret.m32 = m23; ret.m33 = m33;
}

Mat44 Mat44::transpose() const
{
	Mat44 ret;
	transpose(ret);
	return ret;
}

void Mat44::transformPoint(float* point3) const
{
	float x = point3[0];
	float y = point3[1];
	float z = point3[2];

	point3[0] = m00 * x + m01 * y + m02 * z + m03;
	point3[1] = m10 * x + m11 * y + m12 * z + m13;
	point3[2] = m20 * x + m21 * y + m22 * z + m23;
}

void Mat44::transformVector(float* vector3) const
{
	float x = vector3[0];
	float y = vector3[1];
	float z = vector3[2];

	vector3[0] = m00 * x + m01 * y + m02 * z;
	vector3[1] = m10 * x + m11 * y + m12 * z;
	vector3[2] = m20 * x + m21 * y + m22 * z;
}

Mat44 Mat44::makeScale(const float* scale3)
{
	Mat44 ret = identity;
	ret.m00 = scale3[0];
	ret.m11 = scale3[1];
	ret.m22 = scale3[2];
	return ret;
}

Mat44 Mat44::makeAxisRotation(const float* axis3, float angle)
{
	// Reference: OgreMatrix3.cpp
	float c, s;
	c = cosf(angle);
	s = sinf(angle);

	const _Vec3& axis = *reinterpret_cast<const _Vec3*>(axis3);
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

	Mat44 ret = identity;
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

Mat44 Mat44::makeTranslation(const float* translation3)
{
	Mat44 ret = identity;
	ret.m03 = translation3[0];
	ret.m13 = translation3[1];
	ret.m23 = translation3[2];
	return ret;
}

const float _identity[16] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

const Mat44 Mat44::identity = Mat44(_identity);
