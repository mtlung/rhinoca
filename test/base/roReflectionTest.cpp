#include "pch.h"
#include "../../roar/base/roReflection.h"

using namespace ro;

class ReflectionTest {};

struct Shape
{
	float area;
};

struct Circle : public Shape
{
	float radius;
};

struct Vector3
{
	float x, y, z;
};

struct Body
{
	Vector3 position;
	Vector3 velocity;
};

TEST_FIXTURE(ReflectionTest, empty)
{
	reflection.Class<bool>("bool");
	reflection.Class<int>("int");
	reflection.Class<float>("float");
	reflection.Class<double>("double");

	reflection
		.Class<Vector3>("Vector3")
		.field("x", &Vector3::x)
		.field("y", &Vector3::y)
		.field("z", &Vector3::z);

	reflection
		.Class<Shape>("Shape")
		.field("area", &Shape::area);

	reflection
		.Class<Circle, Shape>("Circle")
		.field("radius", &Circle::radius);

	reflection
		.Class<Body>("Body")
		.field("position", &Body::position)
		.field("velocity", &Body::velocity);

	{
		Type* t = reflection.getType<Vector3>();
		Vector3 v = { 1, 2, 3 };
		CHECK_EQUAL(1, *t->getField("x")->getPtr<float>(&v));
		CHECK_EQUAL(2, *t->getField("y")->getPtr<float>(&v));
	}

	{
		Type* t = reflection.getType<Circle>();
		Circle c;
		c.area = 10;
		c.radius = 2;

		CHECK_EQUAL(10, *t->getField("area")->getPtr<float>(&c));
		CHECK_EQUAL(2, *t->getField("radius")->getPtr<float>(&c));
	}
}
