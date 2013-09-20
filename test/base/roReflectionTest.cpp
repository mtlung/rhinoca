#include "pch.h"
#include "../../roar/base/roReflection.h"

using namespace ro;

struct Shape
{
	float area;
};

struct Circle : public Shape
{
	Circle() : pi(3.14f) {}
	float radius;
	const float pi;
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

struct ContainPointer
{
	Body* body;
	const Body* constBody;
};

struct ReflectionTest
{
	ReflectionTest()
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
			.field("radius", &Circle::radius)
			.field("pi", &Circle::pi);

		reflection
			.Class<Body>("Body")
			.field("position", &Body::position)
			.field("velocity", &Body::velocity);

		reflection
			.Class<ContainPointer>("ContainPointer")
			.field("body", &ContainPointer::body)
			.field("constBody", &ContainPointer::constBody);
	}

	~ReflectionTest()
	{
		reflection.reset();
	}
};

TEST_FIXTURE(ReflectionTest, field)
{
	{	// Normal type
		Type* t = reflection.getType<Circle>();

		Field* f = t->getField("radius");
		Field* radius = f;
		CHECK(!f->isConst);
		CHECK(!f->isPointer);
		CHECK_EQUAL(reflection.getType<float>(), f->type);

		f = t->getField("pi");
		Field* pi = f;
		CHECK(f->isConst);
		CHECK(!f->isPointer);
		CHECK_EQUAL(reflection.getType<float>(), f->type);

		// From base class
		f = t->getField("area");
		Field* area = f;
		CHECK(!f->isConst);
		CHECK(!f->isPointer);
		CHECK_EQUAL(reflection.getType<float>(), f->type);

		// Check with getPtr
		Circle c;
		c.area = 10;
		c.radius = 2;

		CHECK_EQUAL(10, *area->getPtr<float>(&c));
		CHECK_EQUAL(10, *area->getConstPtr<float>(&c));
		CHECK_EQUAL(2, *radius->getPtr<float>(&c));
		CHECK_EQUAL(2, *radius->getConstPtr<float>(&c));
		CHECK(!pi->getPtr<float>(&c));
		CHECK_EQUAL(3.14f, *pi->getConstPtr<float>(&c));
	}

	{	// Pointer type
		Type* t = reflection.getType<ContainPointer>();
		Field* f = t->getField("body");
		CHECK(!f->isConst);
		CHECK(f->isPointer);
		CHECK_EQUAL(reflection.getType<Body>(), f->type);
	}
}

roStatus serialize(Type* t, void* ptr)
{
	// Loop for all member field
	if(!t || !ptr) return roStatus::pointer_is_null;

	for(Field* f = t->fields.begin(); f != t->fields.end(); ++f) {

	}

	return roStatus::ok;
}

TEST_FIXTURE(ReflectionTest, serialize)
{
	Circle c;
	c.area = 10;
	c.radius = 2;

	Type* t = reflection.getType<Circle>();
	serialize(t, &c);
}
