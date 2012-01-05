#ifndef __math_roMatrix_h__
#define __math_roMatrix_h__

#include "roVector.h"
#include "../base/roUtility.h"

namespace ro {

static const float matrixEpsilon;

/// Column major
struct Mat4
{
	union {
		// Individual elements
		struct _1 { float
			m00, m10, m20, m30,
			m01, m11, m21, m31,
			m02, m12, m22, m32,
			m03, m13, m23, m33;
		};
		// Columns of Vec4
		struct _2 { float
			c0[4], c1[4], c2[4], c3[4];
		};
		// As a 1 dimension array
		float data[4*4];
		// As a 2 dimension array
		float mat[4][4];
	};

	Mat4();
//	explicit Mat4(const Mat3& rotation, const Vec3& translation);
	explicit Mat4(const float* p16f);
	void		copyFrom(const float* p16f);
	void		copyTo(float* p16f) const;

	const Vec4& operator[](int index) const;
	Vec4& 		operator[](int index);
	Mat4		operator*(float a) const;
	Vec4		operator*(const Vec4& vec) const;
	Vec3		operator*(const Vec3& vec) const;
	Mat4		operator*(const Mat4& a) const;
	Mat4		operator+(const Mat4& a) const;
	Mat4		operator-(const Mat4& a) const;
	Mat4&		operator*=(float a);
	Mat4&		operator*=(const Mat4& a);
	Mat4&		operator+=(const Mat4& a);
	Mat4&		operator-=(const Mat4& a);

	friend Mat4		operator*(float a, const Mat4& mat);
	friend Vec4		operator*(const Vec4& vec, const Mat4& mat);
	friend Vec3		operator*(const Vec3& vec, const Mat4& mat);
	friend Vec4&	operator*=(Vec4& vec, const Mat4& mat);
	friend Vec3&	operator*=(Vec3& vec, const Mat4& mat);

	bool		compare(const Mat4& a) const;					// Exact compare, no epsilon
	bool		compare(const Mat4& a, float epsilon) const;	// Compare with epsilon
	bool		operator==(const Mat4& a) const;				// Exact compare, no epsilon
	bool		operator!=(const Mat4& a) const;				// Exact compare, no epsilon

	void		zero();
	void		identity();
	bool		isIdentity(float epsilon = matrixEpsilon) const;
	bool		isSymmetric(float epsilon = matrixEpsilon) const;
	bool		isDiagonal(float epsilon = matrixEpsilon) const;
	bool		isRotated() const;

	void		projectVector(const Vec4& src, Vec4& dst) const;
	void		unprojectVector(const Vec4& src, Vec4& dst) const;

	float		trace() const;
	float		determinant() const;
	Mat4		transpose() const;		// Returns transpose
	Mat4&		transposeSelf();
	Mat4		inverse() const;		// Returns the inverse (m * m.Inverse() = identity)
	bool		inverseSelf();			// Returns false if determinant is zero
	Mat4		inverseFast() const;	// Returns the inverse (m * m.Inverse() = identity)
	bool		inverseFastSelf();		// Returns false if determinant is zero
	Mat4		transposeMultiply(const Mat4& b) const;

	float*		toFloatPtr() const;
	float*		toFloatPtr();
	const char*	toString(int precision = 2) const;
};	// Mat4

void mat4MulVec4(const float m[4][4], const float src[4], float dst[4]);
void mat4MulMat4(const float lhs[4][4], const float rhs[4][4], float dst[4][4]);


// ----------------------------------------------------------------------

}	// namespace ro

#include "roMatrix.inl"

#endif	// __math_roMatrix_h__