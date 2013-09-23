#include "pch.h"
#include "../../roar/base/roReflection.h"

using namespace ro;
using namespace ro::Reflection;

struct BasicTypes
{
	roUint8		vUint8;
	float		vFloat;
	double		vDouble;
	char*		vStr;
	const char*	vCStr;
	String		vString;
};

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
		ro::registerReflection();

		reflection.Class<BasicTypes>("BasicTypes")
			.field("vUint8", &BasicTypes::vUint8)
			.field("vFloat", &BasicTypes::vFloat)
			.field("vDouble", &BasicTypes::vDouble)
			.field("vStr", &BasicTypes::vStr)
			.field("vCStr", &BasicTypes::vCStr)
			.field("vString", &BasicTypes::vString);

		reflection.Class<Vector3>("Vector3")
			.field("x", &Vector3::x)
			.field("y", &Vector3::y)
			.field("z", &Vector3::z);

		reflection.Class<Shape>("Shape")
			.field("area", &Shape::area);

		reflection.Class<Circle, Shape>("Circle")
			.field("radius", &Circle::radius)
			.field("pi", &Circle::pi);

		reflection.Class<Body>("Body")
			.field("position", &Body::position)
			.field("velocity", &Body::velocity);

		reflection.Class<ContainPointer>("ContainPointer")
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

#include "../../roar/base/roIOStream.h"
TEST_FIXTURE(ReflectionTest, serialize)
{
	JsonSerializer se;
	MemoryOStream os;
	se.writer.setStream(&os);
	se.writer.beginObject();

	{	BasicTypes basicTypes = {
			1u,
			1.23f,
			4.56,
			"Hello world 1",
			"Hello world 2",
			"Hello world 3"
		};

		Type* t = reflection.getType<BasicTypes>();
		t->serialize(se, "Basic types", &basicTypes);
	}

	{	Circle c;
		c.area = 10;
		c.radius = 2;

		Type* t = reflection.getType<Circle>();
		t->serialize(se, "my circle", &c);
	}

	{	Body body;
		Vector3 p = { 0, 1, 2 };
		Vector3 v = { 3, 4, 5 };
		body.position = p;
		body.velocity = v;

		Type* t = reflection.getType<Body>();
		t->serialize(se, "my body", &body);
	}

	se.writer.endObject();
	char* str = (char*)os.bytePtr();
	(void)str;
}
