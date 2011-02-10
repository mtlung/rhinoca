#ifndef __VEC3_H__
#define __VEC3_H__

class Vec3
{
public:
	union {
		struct { float
			x, y, z;
		};
		float data[3];
	};

public:
	/// For performance reasons, default consturctor will not do anything,
	/// so user must aware of it and do proper initialization afterwards if needed.
	inline Vec3() {}

	Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	explicit Vec3(const float* p3f)	{ copyFrom(p3f); }

	void copyFrom(const float* p3f)	{ x = p3f[0]; y = p3f[1]; z = p3f[2]; }

	void copyTo(float* p3f) const	{ p3f[0] = x; p3f[1] = y; p3f[2] = z; }

// Accessors
	float* operator[](unsigned i) { return data; }

	const float* operator[](unsigned i) const { return data; }
};	// Vec3

class Vec4
{
public:
	union {
		struct { float
			x, y, z, w;
		};
		struct { float
			r, g, b, a;
		};
		float data[4];
	};

public:
	/// For performance reasons, default consturctor will not do anything,
	/// so user must aware of it and do proper initialization afterwards if needed.
	inline Vec4() {}

	Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

	explicit Vec4(const float* p4f)	{ copyFrom(p4f); }

	void copyFrom(const float* p4f)	{ x = p4f[0]; y = p4f[1]; z = p4f[2]; w = p4f[3]; }

	void copyTo(float* p4f) const	{ p4f[0] = x; p4f[1] = y; p4f[2] = z; p4f[3] = w; }

// Accessors
	float* operator[](unsigned i) { return data; }

	const float* operator[](unsigned i) const { return data; }
};	// Vec4

#endif	// __VEC3_H__
