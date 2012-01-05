namespace ro {

inline Mat4::Mat4() {
}

inline Mat4::Mat4(const float* p16f) {
	roMemcpy(data, p16f, sizeof(data));
}

inline const Vec4& Mat4::operator[](int index) const {
	roAssert(index >= 0 && index < 4);
	return reinterpret_cast<const Vec4&>(mat[index]);
}

inline Vec4& Mat4::operator[](int index) {
	roAssert(index >= 0 && index < 4);
	return reinterpret_cast<Vec4&>(mat[index]);
}

inline Mat4 operator*(float a, const Mat4& mat) {
	return mat * a;
}

inline Vec4 operator*(const Vec4& vec, const Mat4& mat) {
	return mat * vec;
}

inline Vec3 operator*(const Vec3& vec, const Mat4& mat) {
	return mat * vec;
}

inline Vec4& operator*=(Vec4& vec, const Mat4& mat) {
	vec = mat * vec;
	return vec;
}

inline Vec3& operator*=(Vec3& vec, const Mat4& mat) {
	vec = mat * vec;
	return vec;
}

}	// namespace ro
