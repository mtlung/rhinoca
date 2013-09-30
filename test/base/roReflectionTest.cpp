#include "pch.h"
#include "../../roar/base/roReflection.h"
#include "../../roar/math/roVector.h"

using namespace ro;
using namespace ro::Reflection;

namespace ro {

}

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

struct Body
{
	Vec3 position;
	Vec3 velocity;
};

struct Container
{
	Array<roUint8> intArray;
	Array<Body> bodies;
	Array<Array<float> > floatArray2D;
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

		reflection.Class<Shape>("Shape")
			.field("area", &Shape::area);

		reflection.Class<Circle, Shape>("Circle")
			.field("radius", &Circle::radius)
			.field("pi", &Circle::pi);

		reflection.Class<Body>("Body")
			.field("position", &Body::position)
			.field("velocity", &Body::velocity);

		reflection.Class<Container>("Container")
			.field("intArray", &Container::intArray)
			.field("bodies", &Container::bodies)
			.field("floatArray2D", &Container::floatArray2D);

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

	{	// Array type
		Type* t = reflection.getType<Container>();
		Field* f = t->getField("intArray");
		
		Type* t1 = f->type;
		Type* t2 = f->type->templateArgTypes.front();
		CHECK_EQUAL("Array<roUint8>", t1->name.c_str());
		CHECK_EQUAL("roUint8", t2->name.c_str());
	}

	{	// Array of array
		Type* t = reflection.getType<Container>();
		Field* f = t->getField("floatArray2D");

		Type* t1 = f->type;
		Type* t2 = t1->templateArgTypes.front();
		Type* t3 = t2->templateArgTypes.front();
		CHECK_EQUAL("Array<Array<float>>", t1->name.c_str());
		CHECK_EQUAL("Array<float>", t2->name.c_str());
		CHECK_EQUAL("float", t3->name.c_str());
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

	{	Container container;
		container.intArray.pushBack(1);
		container.intArray.pushBack(2);
		container.intArray.pushBack(3);

		container.bodies.pushBack(Body());

		Array<float> af1;
		af1.pushBack(11);
		af1.pushBack(12);
		container.floatArray2D.pushBack(af1);

		Array<float> af2;
		af2.pushBack(21);
		af2.pushBack(22);
		container.floatArray2D.pushBack(af2);

		Type* t = reflection.getType<Container>();
		t->serialize(se, "My container", &container);
	}

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

	{	Vec3 v(1, 2, 3);
		Type* t = reflection.getType<Vec3>();
		t->serialize(se, "Vec3 type", &v);
	}

	{	Circle c;
		c.area = 10;
		c.radius = 2;

		Type* t = reflection.getType<Circle>();
		t->serialize(se, "my circle", &c);
	}

	{	Body body;
		body.position = Vec3(0, 1, 2);
		body.velocity = Vec3(3, 4, 5);

		Type* t = reflection.getType<Body>();
		t->serialize(se, "my body", &body);
	}

	se.writer.endObject();
	char* str = (char*)os.bytePtr();
	(void)str;
}
