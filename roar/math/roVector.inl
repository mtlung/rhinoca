#include "roMath.h"

namespace ro {

inline Vec2::Vec2() {
}

inline Vec2::Vec2(const float* p2f) {
	this->x = p2f[0];
	this->y = p2f[1];
}

inline Vec2::Vec2(float x, float y) {
	this->x = x;
	this->y = y;
}

inline void Vec2::set(float x, float y) {
	this->x = x;
	this->y = y;
}

inline void Vec2::zero() {
	x = y = 0.0f;
}

inline void Vec2::copyFrom(const float* p2f) {
	x = p2f[0];
	y = p2f[1];
}

inline void Vec2::copyTo(float* p2f) const {
	p2f[0] = x;
	p2f[1] = y;
}

inline bool Vec2::compare(const Vec2& a) const {
	return ((x == a.x) && (y == a.y));
}

inline bool Vec2::compare(const Vec2& a, float epsilon) const {
	if (roFAbs(x - a.x) > epsilon) {
		return false;
	}

	if (roFAbs(y - a.y) > epsilon) {
		return false;
	}

	return true;
}

inline bool Vec2::operator==(const Vec2& a) const {
	return compare(a);
}

inline bool Vec2::operator!=(const Vec2& a) const {
	return !compare(a);
}

inline float Vec2::operator[](int index) const {
	return (&x)[index];
}

inline float& Vec2::operator[](int index) {
	return (&x)[index];
}

inline float Vec2::length() const {
	return roSqrt(x * x + y * y);
}

inline float Vec2::lengthFast() const {
	float sqrLength;

	sqrLength = x * x + y * y;
	return sqrLength * roInvSqrtFast(sqrLength);
}

inline float Vec2::lengthSqr() const {
	return (x * x + y * y);
}

inline float Vec2::normalize() {
	float sqrLength, invLength;

	sqrLength = x * x + y * y;
	invLength = roInvSqrt(sqrLength);
	x *= invLength;
	y *= invLength;
	return invLength * sqrLength;
}

inline float Vec2::normalizeFast() {
	float lengthSqr, invLength;

	lengthSqr = x * x + y * y;
	invLength = roInvSqrtFast(lengthSqr);
	x *= invLength;
	y *= invLength;
	return invLength * lengthSqr;
}

inline void Vec2::snap() {
	x = roFloor(x + 0.5f);
	y = roFloor(y + 0.5f);
}

inline void Vec2::snapInt() {
	x = float(int(x));
	y = float(int(y));
}

inline Vec2 Vec2::operator-() const {
	return Vec2(-x, -y);
}

inline Vec2 Vec2::operator-(const Vec2& a) const {
	return Vec2(x - a.x, y - a.y);
}

inline float Vec2::operator*(const Vec2& a) const {
	return x * a.x + y * a.y;
}

inline Vec2 Vec2::operator*(float a) const {
	return Vec2(x * a, y * a);
}

inline Vec2 Vec2::operator/(float a) const {
	float inva = 1.0f / a;
	return Vec2(x * inva, y * inva);
}

inline Vec2 operator*(float a, const Vec2 b) {
	return Vec2(b.x * a, b.y * a);
}

inline Vec2 Vec2::operator+(const Vec2& a) const {
	return Vec2(x + a.x, y + a.y);
}

inline Vec2& Vec2::operator+=(const Vec2& a) {
	x += a.x;
	y += a.y;

	return *this;
}

inline Vec2& Vec2::operator/=(const Vec2& a) {
	x /= a.x;
	y /= a.y;

	return *this;
}

inline Vec2& Vec2::operator/=(float a) {
	float inva = 1.0f / a;
	x *= inva;
	y *= inva;

	return *this;
}

inline Vec2& Vec2::operator-=(const Vec2& a) {
	x -= a.x;
	y -= a.y;

	return *this;
}

inline Vec2& Vec2::operator*=(float a) {
	x *= a;
	y *= a;

	return *this;
}

inline float *Vec2::toFloatPtr() const {
	return (float*)&x;
}


// ----------------------------------------------------------------------

inline Vec3::Vec3(void) {
}

inline Vec3::Vec3(const float* p3f) {
	this->x = p3f[0];
	this->y = p3f[1];
	this->z = p3f[2];
}

inline Vec3::Vec3(float x, float y, float z) {
	this->x = x;
	this->y = y;
	this->z = z;
}

inline float Vec3::operator[](int index) const {
	return (&x)[index];
}

inline float &Vec3::operator[](int index) {
	return (&x)[index];
}

inline void Vec3::set(float x, float y, float z) {
	this->x = x;
	this->y = y;
	this->z = z;
}

inline void Vec3::zero(void) {
	x = y = z = 0.0f;
}

inline void Vec3::copyFrom(const float* p3f) {
	x = p3f[0];
	y = p3f[1];
	z = p3f[2];
}

inline void Vec3::copyTo(float* p3f) const {
	p3f[0] = x;
	p3f[1] = y;
	p3f[2] = z;
}

inline Vec3 Vec3::operator-() const {
	return Vec3(-x, -y, -z);
}

inline Vec3 Vec3::operator-(const Vec3& a) const {
	return Vec3(x - a.x, y - a.y, z - a.z);
}

inline float Vec3::operator*(const Vec3& a) const {
	return x * a.x + y * a.y + z * a.z;
}

inline Vec3 Vec3::operator*(float a) const {
	return Vec3(x * a, y * a, z * a);
}

inline Vec3 Vec3::operator/(float a) const {
	float inva = 1.0f / a;
	return Vec3(x * inva, y * inva, z * inva);
}

inline Vec3 operator*(float a, const Vec3& b) {
	return Vec3(b.x * a, b.y * a, b.z * a);
}

inline Vec3 Vec3::operator+(const Vec3& a) const {
	return Vec3(x + a.x, y + a.y, z + a.z);
}

inline Vec3& Vec3::operator+=(const Vec3& a) {
	x += a.x;
	y += a.y;
	z += a.z;

	return *this;
}

inline Vec3& Vec3::operator/=(const Vec3& a) {
	x /= a.x;
	y /= a.y;
	z /= a.z;

	return *this;
}

inline Vec3& Vec3::operator/=(float a) {
	float inva = 1.0f / a;
	x *= inva;
	y *= inva;
	z *= inva;

	return *this;
}

inline Vec3& Vec3::operator-=(const Vec3& a) {
	x -= a.x;
	y -= a.y;
	z -= a.z;

	return *this;
}

inline Vec3& Vec3::operator*=(float a) {
	x *= a;
	y *= a;
	z *= a;

	return *this;
}

inline bool Vec3::compare(const Vec3& a) const {
	return ((x == a.x) && (y == a.y) && (z == a.z));
}

inline bool Vec3::compare(const Vec3& a, float epsilon) const {
	if (roFAbs(x - a.x) > epsilon) {
		return false;
	}

	if (roFAbs(y - a.y) > epsilon) {
		return false;
	}

	if (roFAbs(z - a.z) > epsilon) {
		return false;
	}

	return true;
}

inline bool Vec3::operator==(const Vec3& a) const {
	return compare(a);
}

inline bool Vec3::operator!=(const Vec3& a) const {
	return !compare(a);
}

inline float Vec3::normalizeFast(void) {
	float sqrLength, invLength;

	sqrLength = x * x + y * y + z * z;
	invLength = roInvSqrtFast(sqrLength);
	x *= invLength;
	y *= invLength;
	z *= invLength;
	return invLength * sqrLength;
}

inline Vec3 Vec3::cross(const Vec3& a) const {
	return Vec3(y * a.z - z * a.y, z * a.x - x * a.z, x * a.y - y * a.x);
}

inline Vec3& Vec3::cross(const Vec3& a, const Vec3& b) {
	x = a.y * b.z - a.z * b.y;
	y = a.z * b.x - a.x * b.z;
	z = a.x * b.y - a.y * b.x;

	return *this;
}

inline float Vec3::length(void) const {
	return (float)roSqrt(x * x + y * y + z * z);
}

inline float Vec3::lengthSqr(void) const {
	return (x * x + y * y + z * z);
}

inline float Vec3::lengthFast(void) const {
	float sqrLength;

	sqrLength = x * x + y * y + z * z;
	return sqrLength * roInvSqrtFast(sqrLength);
}

inline float Vec3::normalize(void) {
	float sqrLength, invLength;

	sqrLength = x * x + y * y + z * z;
	invLength = roInvSqrt(sqrLength);
	x *= invLength;
	y *= invLength;
	z *= invLength;
	return invLength * sqrLength;
}

inline void Vec3::snap(void) {
	x = roFloor(x + 0.5f);
	y = roFloor(y + 0.5f);
	z = roFloor(z + 0.5f);
}

inline void Vec3::snapInt(void) {
	x = float(int(x));
	y = float(int(y));
	z = float(int(z));
}

inline const Vec2 &Vec3::toVec2(void) const {
	return *reinterpret_cast<const Vec2*>(this);
}

inline Vec2 &Vec3::toVec2(void) {
	return *reinterpret_cast<Vec2*>(this);
}

inline float* Vec3::toFloatPtr(void) const {
	return (float*)&x;
}


// ----------------------------------------------------------------------

inline Vec4::Vec4(void) {
}

inline Vec4::Vec4(const float* p4f) {
	this->x = p4f[0];
	this->y = p4f[1];
	this->z = p4f[2];
	this->w = p4f[3];
}

inline Vec4::Vec4(float x, float y, float z, float w) {
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

inline float Vec4::operator[](int index) const {
	return (&x)[index];
}

inline float &Vec4::operator[](int index) {
	return (&x)[index];
}

inline void Vec4::set(float x, float y, float z, float w) {
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

inline void Vec4::zero(void) {
	x = y = z = w = 0.0f;
}

inline void Vec4::copyFrom(const float* p4f) {
	x = p4f[0];
	y = p4f[1];
	z = p4f[2];
	w = p4f[3];
}

inline void Vec4::copyTo(float* p4f) const {
	p4f[0] = x;
	p4f[1] = y;
	p4f[2] = z;
	p4f[2] = w;
}

inline Vec4 Vec4::operator-() const {
	return Vec4(-x, -y, -z, -w);
}

inline Vec4 Vec4::operator-(const Vec4& a) const {
	return Vec4(x - a.x, y - a.y, z - a.z, w - a.w);
}

inline float Vec4::operator*(const Vec4& a) const {
	return x * a.x + y * a.y + z * a.z + w * a.w;
}

inline Vec4 Vec4::operator*(float a) const {
	return Vec4(x * a, y * a, z * a, w * a);
}

inline Vec4 Vec4::operator/(float a) const {
	float inva = 1.0f / a;
	return Vec4(x * inva, y * inva, z * inva, w * inva);
}

inline Vec4 operator*(float a, const Vec4& b) {
	return Vec4(b.x * a, b.y * a, b.z * a, b.w * a);
}

inline Vec4 Vec4::operator+(const Vec4& a) const {
	return Vec4(x + a.x, y + a.y, z + a.z, w + a.w);
}

inline Vec4& Vec4::operator+=(const Vec4& a) {
	x += a.x;
	y += a.y;
	z += a.z;
	w += a.w;

	return *this;
}

inline Vec4& Vec4::operator/=(const Vec4& a) {
	x /= a.x;
	y /= a.y;
	z /= a.z;
	w /= a.w;

	return *this;
}

inline Vec4& Vec4::operator/=(float a) {
	float inva = 1.0f / a;
	x *= inva;
	y *= inva;
	z *= inva;
	w *= inva;

	return *this;
}

inline Vec4& Vec4::operator-=(const Vec4& a) {
	x -= a.x;
	y -= a.y;
	z -= a.z;
	w -= a.w;

	return *this;
}

inline Vec4& Vec4::operator*=(float a) {
	x *= a;
	y *= a;
	z *= a;
	w *= a;

	return *this;
}

inline bool Vec4::compare(const Vec4& a) const {
	return ((x == a.x) && (y == a.y) && (z == a.z) && (w == a.w));
}

inline bool Vec4::compare(const Vec4& a, float epsilon) const {
	if (roFAbs(x - a.x) > epsilon) {
		return false;
	}

	if (roFAbs(y - a.y) > epsilon) {
		return false;
	}

	if (roFAbs(z - a.z) > epsilon) {
		return false;
	}

	if (roFAbs(w - a.w) > epsilon) {
		return false;
	}

	return true;
}

inline bool Vec4::operator==(const Vec4& a) const {
	return compare(a);
}

inline bool Vec4::operator!=(const Vec4& a) const {
	return !compare(a);
}

inline float Vec4::normalizeFast(void) {
	float sqrLength, invLength;

	sqrLength = x * x + y * y + z * z + w * w;
	invLength = roInvSqrtFast(sqrLength);
	x *= invLength;
	y *= invLength;
	z *= invLength;
	w *= invLength;
	return invLength * sqrLength;
}

inline float Vec4::length(void) const {
	return (float)roSqrt(x * x + y * y + z * z + w * w);
}

inline float Vec4::lengthSqr(void) const {
	return (x * x + y * y + z * z + w * w);
}

inline float Vec4::lengthFast(void) const {
	float sqrLength;

	sqrLength = x * x + y * y + z * z + w * w;
	return sqrLength * roInvSqrtFast(sqrLength);
}

inline float Vec4::normalize(void) {
	float sqrLength, invLength;

	sqrLength = x * x + y * y + z * z + w * w;
	invLength = roInvSqrt(sqrLength);
	x *= invLength;
	y *= invLength;
	z *= invLength;
	return invLength * sqrLength;
}

inline float* Vec4::toFloatPtr(void) const {
	return (float*)&x;
}

}	// namespace ro