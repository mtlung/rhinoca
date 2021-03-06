#ifndef __math_roVector_h__
#define __math_roVector_h__

#include "../platform/roCompiler.h"

namespace ro {

struct Vec2
{
	union {
		struct { float
			x, y;
		};
		float data[2];
	};

	Vec2		();
	Vec2		(const float* p2f);
	Vec2		(float xy);
	Vec2		(float x, float y);

	void 		set(float x, float y);
	void		zero();
	void		copyFrom(const float* p2f);
	void		copyTo(float* p2f) const;

	float		operator[](roSize index) const;
	float&		operator[](roSize index);

// Operator with scaler
	Vec2& 		operator+=(float a);
	Vec2& 		operator-=(float a);
	Vec2& 		operator*=(float a);
	Vec2& 		operator/=(float a);
	friend Vec2	operator+(float a, const Vec2& b);
	friend Vec2	operator+(const Vec2& a, float b);
	friend Vec2	operator-(float a, const Vec2& b);
	friend Vec2	operator-(const Vec2& a, float b);
	friend Vec2	operator*(float a, const Vec2& b);
	friend Vec2	operator*(const Vec2& a, float b);
	friend Vec2	operator/(const Vec2& a, float b);

// Operator with vec
	Vec2		operator-() const;
	Vec2		operator+(const Vec2& a) const;
	Vec2		operator-(const Vec2& a) const;
	Vec2& 		operator+=(const Vec2& a);
	Vec2& 		operator-=(const Vec2& a);
	Vec2& 		operator/=(const Vec2& a);

	bool		compare(const Vec2& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec2& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(	const Vec2& a) const;					// Exact compare, no epsilon
	bool		operator!=(	const Vec2& a) const;					// Exact compare, no epsilon

	float		dot(const Vec2& a) const;
	float		length() const;
	float		lengthFast() const;
	float		lengthSqr() const;
	float		normalize();			// Returns length
	float		normalizeFast();		// Returns length
	Vec2& 		truncate(float length);	// Cap length
	void		clamp(const Vec2& min, const Vec2& max);
	void		snap();					// Snap to closest integer value
	void		snapInt();				// Snap towards integer (floor)

	const char*	toString(int precision = 2) const;

	void		lerp(const Vec2& v1, const Vec2& v2, float l);
};	// Vec2


// ----------------------------------------------------------------------

struct Vec3
{
	union {
		struct { float
			x, y, z;
		};
		float data[3];
	};

	Vec3		();
	Vec3		(const float* p3f);
	Vec3		(float xyz);
	Vec3		(float x, float y, float z);

	void 		set(float x, float y, float z);
	void		zero();
	void		copyFrom(const float* p3f);
	void		copyTo(float* p3f) const;

	float		operator[](roSize index) const;
	float&		operator[](roSize index);

// Operator with scaler
	Vec3& 		operator+=(float a);
	Vec3& 		operator-=(float a);
	Vec3&		operator*=(float a);
	Vec3&		operator/=(float a);
	friend Vec3	operator+(float a, const Vec3& b);
	friend Vec3	operator+(const Vec3& a, float b);
	friend Vec3	operator-(float a, const Vec3& b);
	friend Vec3	operator-(const Vec3& a, float b);
	friend Vec3	operator*(float a, const Vec3& b);
	friend Vec3	operator*(const Vec3& a, float b);
	friend Vec3	operator/(const Vec3& a, float b);

// Operator with vec
	Vec3		operator-() const;
	Vec3		operator+(const Vec3& a) const;
	Vec3		operator-(const Vec3& a) const;
	Vec3&		operator+=(const Vec3& a);
	Vec3&		operator-=(const Vec3& a);
	Vec3&		operator/=(const Vec3& a);

	bool		compare(const Vec3& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec3& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(const Vec3& a) const;					// Exact compare, no epsilon
	bool		operator!=(const Vec3& a) const;					// Exact compare, no epsilon

	bool		fixDegenerateNormal();	// Fix degenerate axial cases
	bool		fixDenormals();			// Change tiny numbers to zero

	float		dot(const Vec3& a) const;
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
	union {
		struct { float
			x, y, z, w;
		};
		float data[4];
	};

	Vec4		();
	Vec4		(const float* p4f);
	Vec4		(float xyzw);
	Vec4		(float x, float y, float z, float w);

	void 		set(float x, float y, float z, float w);
	void		zero();
	void		copyFrom(const float* p4f);
	void		copyTo(float* p4f) const;

	float		operator[](roSize index) const;
	float&		operator[](roSize index);

// Operator with scaler
	Vec4&		operator+=(float a);
	Vec4&		operator-=(float a);
	Vec4&		operator*=(float a);
	Vec4&		operator/=(float a);
	friend Vec4	operator+(float a, const Vec4& b);
	friend Vec4	operator+(const Vec4& a, float b);
	friend Vec4	operator-(float a, const Vec4& b);
	friend Vec4	operator-(const Vec4& a, float b);
	friend Vec4	operator*(float a, const Vec4& b);
	friend Vec4	operator*(const Vec4& a, float b);
	friend Vec4	operator/(const Vec4& a, float b);

// Operator with vec
	Vec4		operator-() const;
	Vec4		operator+(const Vec4& a) const;
	Vec4		operator-(const Vec4& a) const;
	Vec4&		operator+=(const Vec4& a);
	Vec4&		operator-=(const Vec4& a);
	Vec4&		operator/=(const Vec4& a);

	bool		compare(const Vec4& a) const;						// Exact compare, no epsilon
	bool		compare(const Vec4& a, float epsilon) const;		// Compare with epsilon
	bool		operator==(const Vec4& a) const;					// Exact compare, no epsilon
	bool		operator!=(const Vec4& a) const;					// Exact compare, no epsilon

	float		dot(const Vec4& a) const;
	float		length() const;
	float		lengthSqr() const;
	float		lengthFast() const;
	float		normalize();			// Returns length
	float		normalizeFast();		// Returns length

	const char*	toString(int precision = 2) const;

	void		lerp(const Vec4& v1, const Vec4& v2, float l);
};	// Vec4

}	// namespace ro

#include "roVector.inl"

#endif	// __math_roVector_h__
