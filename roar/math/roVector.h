#ifndef __math_roVector_h__
#define __math_roVector_h__

namespace ro {

struct Vec2
{
	float		x, y;

	Vec2		();
	Vec2		(const float* p2f);
	Vec2		(float x, float y);

	void 		set(float x, float y);
	void		zero();
	void		copyFrom(const float* p2f);
	void		copyTo(float* p2f) const;

	float		operator[](int index) const;
	float&		operator[](int index);
	Vec2		operator-() const;
	float		operator*(const Vec2& a) const;
	Vec2		operator*(float a) const;
	Vec2		operator/(float a) const;
	Vec2		operator+(const Vec2& a) const;
	Vec2		operator-(const Vec2& a) const;
	Vec2& 		operator+=(const Vec2& a);
	Vec2& 		operator-=(const Vec2& a);
	Vec2& 		operator/=(const Vec2& a);
	Vec2& 		operator/=(float a);
	Vec2& 		operator*=(float a);

	friend Vec2	operator*(float a, const Vec2 b);

	bool		compare(const Vec2& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec2& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(	const Vec2& a) const;					// Exact compare, no epsilon
	bool		operator!=(	const Vec2& a) const;					// Exact compare, no epsilon

	float		length() const;
	float		lengthFast() const;
	float		lengthSqr() const;
	float		normalize();			// Returns length
	float		normalizeFast();		// Returns length
	Vec2& 		truncate(float length);	// Cap length
	void		clamp(const Vec2& min, const Vec2& max);
	void		snap();					// Snap to closest integer value
	void		snapInt();				// Snap towards integer (floor)

	float*		toFloatPtr() const;
	const char*	toString(int precision = 2) const;

	void		lerp(const Vec2& v1, const Vec2& v2, float l);
};	// Vec2


// ----------------------------------------------------------------------

struct Vec3
{
	float		x, y, z;

	Vec3		();
	Vec3		(const float* p3f);
	Vec3		(float x, float y, float z);

	void 		set(float x, float y, float z);
	void		zero();
	void		copyFrom(const float* p3f);
	void		copyTo(float* p3f) const;

	float		operator[](int index) const;
	float&		operator[](int index);
	Vec3		operator-() const;
	float		operator*(const Vec3& a) const;
	Vec3		operator*(float a) const;
	Vec3		operator/(float a) const;
	Vec3		operator+(const Vec3& a) const;
	Vec3		operator-(const Vec3& a) const;
	Vec3&		operator+=(const Vec3& a);
	Vec3&		operator-=(const Vec3& a);
	Vec3&		operator/=(const Vec3& a);
	Vec3&		operator/=(float a);
	Vec3&		operator*=(float a);

	friend Vec3	operator*(float a, const Vec3& b);

	bool		compare(const Vec3& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec3& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(const Vec3& a) const;					// Exact compare, no epsilon
	bool		operator!=(const Vec3& a) const;					// Exact compare, no epsilon

	bool		fixDegenerateNormal();	// Fix degenerate axial cases
	bool		fixDenormals();			// Change tiny numbers to zero

	Vec3		cross(const Vec3& a) const;
	Vec3&		cross(const Vec3& a, const Vec3& b);
	float		length() const;
	float		lengthSqr() const;
	float		lengthFast() const;
	float		normalize();			// Returns length
	float		normalizeFast();		// Returns length
	Vec3&		truncate(float length);	// Cap length
	void		clamp(const Vec3& min, const Vec3& max);
	void		snap();					// Snap to closest integer value
	void		snapInt();				// Snap towards integer (floor)

	float		toYaw() const;
	float		toPitch() const;
//	idAngles	toAngles() const;
//	idPolar3	toPolar() const;
//	Mat3		toMat3() const;			// vector should be normalized
	const Vec2&	toVec2() const;
	Vec2&		toVec2();
	float*		toFloatPtr() const;
	const char*	toString(int precision = 2) const;

	void		normalVectors(Vec3& left, Vec3& down) const;	// vector should be normalized
	void		orthogonalBasis(Vec3& left, Vec3& up) const;

	void		projectOntoPlane(const Vec3& normal, float overBounce = 1.0f);
	bool		projectAlongPlane(const Vec3& normal, float epsilon, float overBounce = 1.0f);
	void		projectSelfOntoSphere(float radius);

	void		lerp(const Vec3& v1, const Vec3& v2, float l);
	void		sLerp(const Vec3& v1, const Vec3& v2, float l);
};	// Vec3


// ----------------------------------------------------------------------

struct Vec4
{
	float		x, y, z, w;

	Vec4		();
	Vec4		(const float* p4f);
	Vec4		(float x, float y, float z, float w);

	void 		set(float x, float y, float z, float w);
	void		zero();
	void		copyFrom(const float* p4f);
	void		copyTo(float* p4f) const;

	float		operator[](int index) const;
	float&		operator[](int index);
	Vec4		operator-() const;
	float		operator*(const Vec4& a) const;
	Vec4		operator*(float a) const;
	Vec4		operator/(float a) const;
	Vec4		operator+(const Vec4& a) const;
	Vec4		operator-(const Vec4& a) const;
	Vec4&		operator+=(const Vec4& a);
	Vec4&		operator-=(const Vec4& a);
	Vec4&		operator/=(const Vec4& a);
	Vec4&		operator/=(float a);
	Vec4&		operator*=(float a);

	friend Vec4	operator*(float a, const Vec4& b);

	bool		compare(const Vec4& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec4& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(const Vec4& a) const;					// Exact compare, no epsilon
	bool		operator!=(const Vec4& a) const;					// Exact compare, no epsilon

	float		length() const;
	float		lengthSqr() const;
	float		lengthFast() const;
	float		normalize();			// Returns length
	float		normalizeFast();		// Returns length

	float*		toFloatPtr() const;
	const char*	toString(int precision = 2) const;

	void		lerp(const Vec4& v1, const Vec4& v2, float l);
};	// Vec4

}	// namespace ro

#include "roVector.inl"

#endif	// __math_roVector_h__
