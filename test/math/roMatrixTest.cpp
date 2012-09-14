#include "pch.h"
#include "../../roar/math/roMatrix.h"

using namespace ro;

struct MatrixTest {};

template<typename T>
static bool isAlmostEqual(const T& lhs, const T& rhs, float eps)
{
	return (lhs - rhs).length() < eps;
}

TEST_FIXTURE(MatrixTest, transformVector)
{
	float trans[] = { 1, 2, 3 };
	Mat4 translation = makeTranslationMat4(trans);

	// Anti-clockwise around y-axis
	float axis[] = { 0, 1, 0 };
	Mat4 rotation = makeAxisRotationMat4(axis, roDeg2Rad(90));

	float scale_ [] = { 2, 2, 2 };
	Mat4 scale = makeScaleMat4(scale_);

	Mat4 rot_tran = translation * rotation;
	Mat4 tran_rot = rotation * translation;

	// Vec3
	{	Vec3 v = Vec3(1, 0, 0);
		v = translation * v;
		CHECK(v == Vec3(2, 2, 3));
	}

	{	Vec3 v = Vec3(1, 0, 0);
		v = rotation * v;
		CHECK(isAlmostEqual(v, Vec3(0, 0, -1), roFLT_EPSILON));
	}

	{	Vec3 v = Vec3(1, 2, 3);
		v = scale * v;
		CHECK(isAlmostEqual(v, Vec3(2, 4, 6), roFLT_EPSILON));
	}

	{	Vec3 v = Vec3(1, 0, 0);
		v = rot_tran * v;
		CHECK(isAlmostEqual(v, Vec3(1, 2, 2), roFLT_EPSILON));
	}

	{	Vec3 v = Vec3(1, 0, 0);
		v = tran_rot * v;
		CHECK(isAlmostEqual(v, Vec3(3, 2, -2), roFLT_EPSILON*2));
	}

	// Vec4
	{	Vec4 v = Vec4(1, 0, 0, 1);
		v = translation * v;
		CHECK(v == Vec4(2, 2, 3, 1));
	}

	{	Vec4 v = Vec4(1, 0, 0, 1);
		v = rotation * v;
		CHECK(isAlmostEqual(v, Vec4(0, 0, -1, 1), roFLT_EPSILON));
	}

	{	Vec4 v = Vec4(1, 0, 0, 1);
		v = rot_tran * v;
		CHECK(isAlmostEqual(v, Vec4(1, 2, 2, 1), roFLT_EPSILON));
	}

	{	Vec4 v = Vec4(1, 0, 0, 1);
		v = tran_rot * v;
		CHECK(isAlmostEqual(v, Vec4(3, 2, -2, 1), roFLT_EPSILON*2));
	}
}
