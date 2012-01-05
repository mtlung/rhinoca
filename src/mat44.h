#ifndef __MAT44_H__
#define __MAT44_H__

/// A 4 by 4 matrix, column-major, column-order (vector-on-the right).
/// Vector multiplication: column-order, matrix * column vector, tradition math representation.
/// Meory storage order: column-order, in order to keep the memory data DirectX and OpenGL compitable if we use column major.
///
/// If the matrix is used as a transformation matrix, it is assumed to be
/// a SRT transform, that is: Scaling, Rotation and Translation.
///
/// Reference: Demystifying Matrix Layouts
/// http://www.allbusiness.com/science-technology/mathematics/12809327-1.html
class Mat44
{
public:
	union {
		// Individual elements
		struct { float
			m00, m10, m20, m30,
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
		float data2D[4][4];
	};

public:
	/// For performance reasons, default constructor will not do anything,
	/// so user must aware of it and do proper initialization afterwards if needed.
	inline Mat44() {}

	explicit Mat44(const float* data) { copyFrom(data); }

	void copyFrom(const float* data);

	void copyTo(float* data) const;

// Accessors
	float* operator[](unsigned iColumn) { return data2D[iColumn]; }

	const float* operator[](unsigned iColumn) const { return data2D[iColumn]; }

// Matrix multiplication
	void mul(const Mat44& rhs, Mat44& result) const;

	Mat44 operator*(const Mat44& rhs) const;
	Mat44& operator*=(const Mat44& rhs);
	Mat44& operator*=(float rhs);

// Vector multiplication
	void mul(const float* rhs4, float* result4) const;

// Transpose
	void transpose(Mat44& result) const;
	Mat44 transpose() const;

// Transform point and vector
	void transformPoint(float* point3) const;

	void transformVector(float* vector3) const;

// Make transforms
	static Mat44 makeScale(const float* scale3);

	static Mat44 makeAxisRotation(const float* axis3, float angle);

	static Mat44 makeTranslation(const float* translation3);

	static const Mat44 identity;
};	// Mat44

#endif	// __MAT44_H__
