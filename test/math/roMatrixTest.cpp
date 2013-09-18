#include "pch.h"
#include "../../roar/math/roMatrix.h"

using namespace ro;

struct MatrixTest {
	MatrixTest()
		: ma(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
		, mb(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
		, mc(15)
		, md(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30)
	{}
	Mat4 ma, mb, mc, md;
};

template<typename T>
static bool isAlmostEqual(const T& lhs, const T& rhs, float eps)
{
	return (lhs - rhs).length() < eps;
}

TEST_FIXTURE(MatrixTest, basic)
{
	CHECK_EQUAL(ma.m01, ma[1][0]);
	CHECK_EQUAL(ma.data[1], ma[0][1]);

	CHECK_EQUAL(4u, ma.rows());
	CHECK_EQUAL(4u, ma.columns());

	CHECK(ma == ma);
	CHECK(ma != mb);

	{	float buffer[4][4];
		ma.copyTo((float*)buffer);
		Mat4 m;
		m.copyFrom((float*)buffer);
		CHECK(m == ma);
	}
}

TEST_FIXTURE(MatrixTest, negation)
{
	CHECK(-ma == Mat4(0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15));
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

TEST_FIXTURE(MatrixTest, determinant)
{
	Mat4 a(
		1, 2, 1, 1,
		2, 2, 2, 2,
		2, 1, 1, 2,
		2, 1, 0, 1);

	CHECK_EQUAL(-2, a.determinant());
	CHECK_EQUAL(-2, (a + 0).determinant());
}

TEST_FIXTURE(MatrixTest, inverse)
{
	Mat4 a(
		1, 2, 1, 1,
		2, 2, 2, 2,
		2, 1, 1, 2,
		2, 1, 0, 1);

	Mat4 b(
		-1,     1, -1,  1,
		 1, -0.5f,  0,  0,
		-1,  1.5f, -1,  0,
		 1, -1.5f,  2, -1);

	Mat4 ai;
	CHECK(a.inverse(ai));

	CHECK(ai == b);
	Mat4 tmp;
	CHECK((a * 1).inverse(tmp));
	CHECK(ai == tmp);

	CHECK(a * ai == Mat4::identity);
	CHECK(ai * a == Mat4::identity);
}
