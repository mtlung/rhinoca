namespace ro {

inline Mat4::Mat4() {
}

inline Mat4::Mat4(const Mat4& a) {
	roMemcpy(data, a.data, sizeof(data));
}

inline Mat4::Mat4(const float* p16f) {
	roMemcpy(data, p16f, sizeof(data));
}

inline Mat4::Mat4(
	 float xx, float xy, float xz, float xw,
	 float yx, float yy, float yz, float yw,
	 float zx, float zy, float zz, float zw,
	 float wx, float wy, float wz, float ww)
{
	mat[0][0] = xx; mat[1][0] = xy; mat[2][0] = xz; mat[3][0] = xw;
	mat[0][1] = yx; mat[1][1] = yy; mat[2][1] = yz; mat[3][1] = yw;
	mat[0][2] = zx; mat[1][2] = zy; mat[2][2] = zz; mat[3][2] = zw;
	mat[0][3] = wx; mat[1][3] = wy; mat[2][3] = wz; mat[3][3] = ww;
}

inline void Mat4::copyFrom(const float* p16f) {
	roMemcpy(data, p16f, sizeof(data));
}

inline void Mat4::copyTo(float* p16f) const {
	roMemcpy(p16f, data, sizeof(data));
}

inline const Vec4& Mat4::operator[](roSize colIndex) const {
	roAssert(colIndex >= 0 && colIndex < 4);
	return reinterpret_cast<const Vec4&>(mat[colIndex]);
}

inline Vec4& Mat4::operator[](roSize colIndex) {
	roAssert(colIndex >= 0 && colIndex < 4);
	return reinterpret_cast<Vec4&>(mat[colIndex]);
}

// Operator with scaler
inline Mat4 operator+(float a, const Mat4& mat) {
	return mat + a;
}

inline Mat4 operator*(float a, const Mat4& mat) {
	return mat * a;
}

// Operator with vec
inline Vec2 operator*(const Mat4& m, const Vec2& vec) {
	Vec2 ret;
	mat4MulVec2(m.mat, vec.data, ret.data);
	return ret;
}

inline Vec3 operator*(const Mat4& m, const Vec3& vec) {
	Vec3 ret;
	mat4MulVec3(m.mat, vec.data, ret.data);
	return ret;
}

inline Vec4 operator*(const Mat4& m, const Vec4& vec) {
	Vec4 ret;
	mat4MulVec4(m.mat, vec.data, ret.data);
	return ret;
}

inline Vec2 operator*(const Vec2& vec, const Mat4& mat) {
	return mat * vec;
}

inline Vec3 operator*(const Vec3& vec, const Mat4& mat) {
	return mat * vec;
}

inline Vec4 operator*(const Vec4& vec, const Mat4& mat) {
	return mat * vec;
}

inline Vec2& operator*=(Vec2& vec, const Mat4& mat) {
	vec = mat * vec;
	return vec;
}

inline Vec3& operator*=(Vec3& vec, const Mat4& mat) {
	vec = mat * vec;
	return vec;
}

inline Vec4& operator*=(Vec4& vec, const Mat4& mat) {
	vec = mat * vec;
	return vec;
}

// Operator with mat
inline Mat4 Mat4::operator*(const Mat4& a) const {
	Mat4 ret;
	mat4MulMat4(mat, a.mat, ret.mat);
	return ret;
}

inline Mat4& Mat4::operator*=(const Mat4& a) {
	*this = (*this) * a;
	return *this;
}

inline Mat4& Mat4::operator+=(const Mat4& a) {
	return *this = *this + a;
}

inline Mat4& Mat4::operator-=(const Mat4& a) {
	return *this = *this - a;
}

inline bool Mat4::compare(const Mat4& a) const {
	return compare(a, matrixEpsilon);
}

inline bool Mat4::operator!=(const Mat4& a) const {
	return !(*this == a);
}

inline bool Mat4::inverse(Mat4& dst, float epsilon) const {
	return mat4Inverse(dst.mat, mat, epsilon);
}

inline bool Mat4::inverseSelf(float epsilon) {
	return mat4Inverse(mat, mat, epsilon);
}

inline void Mat4::toZero() {
	for(roSize i=0; i<roCountof(data); ++i)
		data[i] = 0;
}

inline void Mat4::toIdentity() {
	*this = Mat4::identity;
}

}	// namespace ro
