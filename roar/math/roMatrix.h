#ifndef __math_roMatrix_h__
#define __math_roMatrix_h__

#include "roVector.h"
#include "../base/roUtility.h"

namespace ro {

static float matrixEpsilon;

/// Column major
/// Matrix * column vector
///
/// Translation layout:
/// 1 0 0 X
/// 0 1 0 Y
/// 0 0 1 Z
/// 0 0 0 1
struct Mat4
{
	union {
		// Individual elements
		struct { float
			m00, m10, m20, m30,	// NOTE: m12 means element of row 1, column 2 
			m01, m11, m21, m31,
			m02, m12, m22, m32,
			m03, m13, m23, m33;
		};
		// Columns of Vec4
		struct { float
			c0[4], c1[4], c2[4], c3[4];
		};
		// As a 1 dimension array
		float data[4*4];
		// As a 2 dimension array
		float mat[4][4];
	};

	Mat4();
//	explicit Mat4(const Mat3& rotation, const Vec3& translation);
	Mat4(const Mat4& a);
	explicit Mat4(const float* p16f);
	Mat4(
		float xx, float xy, float xz, float xw,
		float yx, float yy, float yz, float yw,
		float zx, float zy, float zz, float zw,
		float wx, float wy, float wz, float ww);

	void		copyFrom(const float* p16f);
	void		copyTo(float* p16f) const;

	const Vec4& operator[](roSize colIndex) const;
	Vec4& 		operator[](roSize colIndex);
	Mat4		operator*(float a) const;
	Vec2		operator*(const Vec2& vec) const;
	Vec3		operator*(const Vec3& vec) const;
	Vec4		operator*(const Vec4& vec) const;
	Mat4		operator*(const Mat4& a) const;
	Mat4		operator+(const Mat4& a) const;
	Mat4		operator-(const Mat4& a) const;
	Mat4&		operator*=(float a);
	Mat4&		operator*=(const Mat4& a);
	Mat4&		operator+=(const Mat4& a);
	Mat4&		operator-=(const Mat4& a);

	friend Mat4		operator*(float a, const Mat4& mat);
	friend Vec2		operator*(const Vec2& vec, const Mat4& mat);
	friend Vec3		operator*(const Vec3& vec, const Mat4& mat);
	friend Vec4		operator*(const Vec4& vec, const Mat4& mat);
	friend Vec2&	operator*=(Vec2& vec, const Mat4& mat);
	friend Vec3&	operator*=(Vec3& vec, const Mat4& mat);
	friend Vec4&	operator*=(Vec4& vec, const Mat4& mat);

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

	const char*	toString(int precision = 2) const;
};	// Mat4

extern const Mat4 mat4Identity;

void mat4MulVec2(const float m[4][4], const float src[2], float dst[2]);
void mat4MulVec3(const float m[4][4], const float src[3], float dst[3]);
void mat4MulVec4(const float m[4][4], const float src[4], float dst[4]);
void mat4MulMat4(const float lhs[4][4], const float rhs[4][4], float dst[4][4]);

Mat4 makeScaleMat4(const float scale[3]);
Mat4 makeAxisRotationMat4(const float axis[3], float angleRad);
Mat4 makeTranslationMat4(const float translation[3]);
Mat4 makeOrthoMat4(float left, float right, float bottom, float top, float zNear, float zFar);
Mat4 makePrespectiveMat4(float fovyDeg, float acpectWidthOverHeight, float zNera, float zFar);

// ----------------------------------------------------------------------

}	// namespace ro

#include "roMatrix.inl"

#endif	// __math_roMatrix_h__
