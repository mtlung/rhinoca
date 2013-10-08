#include "pch.h"
#include "roMatrix.h"
#include "../base/roReflection.h"
#include "../base/roUtility.h"
#include "../platform/roCompiler.h"
#include "../platform/roOS.h"

#if roCPU_SSE
#	include <xmmintrin.h>
#endif

namespace ro {

float matrixEpsilon = 1e-20f;

const Mat4 Mat4::identity(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
);

Mat4::Mat4(float scalar)
{
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		mat[i][j] = scalar;
}

// Operator with scaler
Mat4& Mat4::operator*=(float a)
{
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		mat[i][j] *= a;
	return *this;
}

Mat4 operator+(const Mat4& m, float a)
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = m.mat[i][j] + a;
	return ret;
}

Mat4 operator-(const Mat4& m, float a)
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = m.mat[i][j] - a;
	return ret;
}

Mat4 operator-(float a, const Mat4& m)
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = a - m.mat[i][j];
	return ret;
}

Mat4 operator*(const Mat4& m, float a)
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = m.mat[i][j] * a;
	return ret;
}

// Operator with mat
Mat4 Mat4::operator-() const
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = -mat[i][j];
	return ret;
}

Mat4 Mat4::operator+(const Mat4& a) const
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = mat[i][j] + a.mat[i][j];
	return ret;
}

Mat4 Mat4::operator-(const Mat4& a) const
{
	Mat4 ret;
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		ret.mat[i][j] = mat[i][j] - a.mat[i][j];
	return ret;
}

bool Mat4::compare(const Mat4& a, float epsilon) const
{
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		if(!roIsNearEqual(mat[i][j], a.mat[i][j], epsilon))
			return false;
	return true;
}

bool Mat4::operator==(const Mat4& a) const
{
	for(roSize i=0; i<4; ++i) for(roSize j=0; j<4; ++j)
		if(mat[i][j] != a.mat[i][j])
			return false;
	return true;
}

float Mat4::determinant() const
{
	typedef float T;
	const T d12 = m20 * m31 - m30 * m21;
	const T d13 = m20 * m32 - m30 * m22;
	const T d23 = m21 * m32 - m31 * m22;
	const T d24 = m21 * m33 - m31 * m23;
	const T d34 = m22 * m33 - m32 * m23;
	const T d41 = m23 * m30 - m33 * m20;

	return
		m00 *  (m11 * d34 - m12 * d24 + m13 * d23) +
		m01 * -(m10 * d34 + m12 * d41 + m13 * d13) +
		m02 *  (m10 * d24 + m11 * d41 + m13 * d12) +
		m03 * -(m10 * d23 - m11 * d13 + m12 * d12);
}

Mat4 Mat4::transpose() const
{
	Mat4 transpose;

	for(roSize i=0; i<4; ++i)
		for(roSize j=0; j<4; ++j)
			transpose[i][j] = mat[j][i];

	return transpose;
}

Mat4& Mat4::transposeSelf()
{
	for(roSize i=0; i<4; ++i)
		for(roSize j=0; j<4; ++j)
			roSwap(mat[i][j], mat[j][i]);

	return *this;
}

void mat4MulVec2(const float m[4][4], const float src[2], float dst[2])
{
	// Local variables to prevent parameter aliasing
	const float x = src[0];
	const float y = src[1];

	dst[0] = m[0][0] * x + m[0][1] * y + m[0][3];
	dst[1] = m[1][0] * x + m[1][1] * y + m[1][3];
}

void mat4MulVec3(const float m[4][4], const float src[3], float dst[3])
{
	// Local variables to prevent parameter aliasing
	const float x = src[0];
	const float y = src[1];
	const float z = src[2];

	float scale = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
	if(scale == 0.0f) {
		dst[0] = dst[1] = dst[2] = 0;
	}
	else if(scale == 1.0f) {
		dst[0] = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
		dst[1] = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
		dst[2] = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
	}
	else {
		float invS = 1.0f / scale;
		dst[0] = (m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0]) * invS;
		dst[1] = (m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1]) * invS;
		dst[2] = (m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]) * invS;
	}
}

void mat4MulVec4(const float m[4][4], const float src[4], float dst[4])
{
#if roCPU_SSEd
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

	dst[0] = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0] * w;
	dst[1] = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1] * w;
	dst[2] = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2] * w;
	dst[3] = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3] * w;
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

	for(roSize i=0; i<4; ++i) {
		for(roSize j=0; j<4; ++j) {
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

bool mat4Inverse(float dst[4][4], const float src[4][4], float epsilon)
{
	typedef float T;

	T m00 = src[0][0];	T m01 = src[0][1];	T m02 = src[0][2];	T m03 = src[0][3];
	T m10 = src[1][0];	T m11 = src[1][1];	T m12 = src[1][2];	T m13 = src[1][3];
	T m20 = src[2][0];	T m21 = src[2][1];	T m22 = src[2][2];	T m23 = src[2][3];
	T m30 = src[3][0];	T m31 = src[3][1];	T m32 = src[3][2];	T m33 = src[3][3];

	const T k2031 = m20 * m31 - m21 * m30;
	const T k2032 = m20 * m32 - m22 * m30;
	const T k2033 = m20 * m33 - m23 * m30;
	const T k2132 = m21 * m32 - m22 * m31;
	const T k2133 = m21 * m33 - m23 * m31;
	const T k2233 = m22 * m33 - m23 * m32;

	const T k1031 = m10 * m31 - m11 * m30;
	const T k1032 = m10 * m32 - m12 * m30;
	const T k1033 = m10 * m33 - m13 * m30;
	const T k1132 = m11 * m32 - m12 * m31;
	const T k1133 = m11 * m33 - m13 * m31;
	const T k1233 = m12 * m33 - m13 * m32;

	const T k2110 = m21 * m10 - m20 * m11;
	const T k2210 = m22 * m10 - m20 * m12;
	const T k2310 = m23 * m10 - m20 * m13;
	const T k2211 = m22 * m11 - m21 * m12;
	const T k2311 = m23 * m11 - m21 * m13;
	const T k2312 = m23 * m12 - m22 * m13;

	const T t00 = + (k2233 * m11 - k2133 * m12 + k2132 * m13);
	const T t10 = - (k2233 * m10 - k2033 * m12 + k2032 * m13);
	const T t20 = + (k2133 * m10 - k2033 * m11 + k2031 * m13);
	const T t30 = - (k2132 * m10 - k2032 * m11 + k2031 * m12);

	const T det = t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03;

	// We may got a very small determinant if the scaling part of the matrix is small
	if(roIsNearZero(det, epsilon))
		return false;

	const T invDet = T(1) / det;

	dst[0][0] = t00 * invDet;
	dst[1][0] = t10 * invDet;
	dst[2][0] = t20 * invDet;
	dst[3][0] = t30 * invDet;

	dst[0][1] = - (k2233 * m01 - k2133 * m02 + k2132 * m03) * invDet;
	dst[1][1] = + (k2233 * m00 - k2033 * m02 + k2032 * m03) * invDet;
	dst[2][1] = - (k2133 * m00 - k2033 * m01 + k2031 * m03) * invDet;
	dst[3][1] = + (k2132 * m00 - k2032 * m01 + k2031 * m02) * invDet;

	dst[0][2] = + (k1233 * m01 - k1133 * m02 + k1132 * m03) * invDet;
	dst[1][2] = - (k1233 * m00 - k1033 * m02 + k1032 * m03) * invDet;
	dst[2][2] = + (k1133 * m00 - k1033 * m01 + k1031 * m03) * invDet;
	dst[3][2] = - (k1132 * m00 - k1032 * m01 + k1031 * m02) * invDet;

	dst[0][3] = - (k2312 * m01 - k2311 * m02 + k2211 * m03) * invDet;
	dst[1][3] = + (k2312 * m00 - k2310 * m02 + k2210 * m03) * invDet;
	dst[2][3] = - (k2311 * m00 - k2310 * m01 + k2110 * m03) * invDet;
	dst[3][3] = + (k2211 * m00 - k2210 * m01 + k2110 * m02) * invDet;

	return true;
}

Mat4 makeScaleMat4(const float scale[3])
{
	Mat4 ret(Mat4::identity);
	ret.m00 = scale[0];
	ret.m11 = scale[1];
	ret.m22 = scale[2];
	return ret;
}

Mat4 makeAxisRotationMat4(const float _axis[3], float angleRad)
{
	// Reference: OgreMatrix3.cpp
	float c, s;
	c = cosf(angleRad);
	s = sinf(angleRad);

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

	Mat4 ret(Mat4::identity);
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
	Mat4 ret(Mat4::identity);
	ret.m03 = translation[0];
	ret.m13 = translation[1];
	ret.m23 = translation[2];
	return ret;
}

Mat4 makeOrthoMat4(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Mat4 m(Mat4::identity);

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
	m.toZero();

	m.m00 = f / acpectWidthOverHeight;	// One over width
	m.m11 = f;							// One over height
	m.m22 = (zFar + zNear) * nf;
	m.m23 = (2 * zFar * zNear) * nf;
	m.m32 = -1;							// Perspective division

	return m;
}

roStatus Reflection::serialize_mat4(Serializer& se, Field& field, void* fieldParent)
{
	Mat4* m = (Mat4*)field.getConstPtr<Vec4>(fieldParent);
	if(!m) return roStatus::pointer_is_null;

	roStatus st = se.beginArray(field, m->rows() * m->columns());
	if(!st) return st;

	for(roSize i=0, end=m->rows() * m->columns(); i<end; ++i) {
		st = se.serialize(*(m->data + i));
		if(!st) return st;
	}

	return se.endArray();
}

}	// namespace ro
