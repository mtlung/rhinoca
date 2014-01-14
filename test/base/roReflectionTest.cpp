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
	Circle& operator=(const Circle& rhs) { radius = rhs.radius; return *this; }
	float radius;
	const float pi;
};

struct Body
{
	Body() : position(0.0f), velocity(0.0f) {}
	Vec3 position;
	Vec3 velocity;
};

struct MyListNode : public ListNode<MyListNode>
{
	MyListNode() {}
	MyListNode(const roUtf8* name) : name(name) {}
	String name;
};

typedef LinkList<MyListNode> MyLinkList;

struct Container
{
	Array<roUint8> intArray;
	Array<Body> bodies;
	TinyArray<roUint8, 4> tinyArray;
	Array<Array<float> > floatArray2D;
	MyLinkList linkList;
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

		reflection.Class<MyListNode>("MyListNode")
			.field("name", &MyListNode::name);

		reflection.Class<Container>("Container")
			.field("intArray", &Container::intArray)
			.field("bodies", &Container::bodies)
			.field("tinyArray", &Container::tinyArray)
			.field("floatArray2D", &Container::floatArray2D)
			.field("linkList", &Container::linkList);

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
		CHECK_EQUAL("IArray<roUint8>", t1->name.c_str());
		CHECK_EQUAL("roUint8", t2->name.c_str());
	}

	{	// Array of array
		Type* t = reflection.getType<Container>();
		Field* f = t->getField("floatArray2D");

		Type* t1 = f->type;
		Type* t2 = t1->templateArgTypes.front();
		Type* t3 = t2->templateArgTypes.front();
		CHECK_EQUAL("IArray<IArray<float>>", t1->name.c_str());
		CHECK_EQUAL("IArray<float>", t2->name.c_str());
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
#include "../../roar/base/roJsonSerializer.h"
TEST_FIXTURE(ReflectionTest, serialize)
{
	MemoryOStream os;

	// Write
	{	JsonOutputSerializer ose;
		ose.writer.setStream(&os);
		ose.beginArchive();

		{	Container container;
			container.intArray.pushBack(1);
			container.intArray.pushBack(2);
			container.intArray.pushBack(3);

			Body body;
			body.position = Vec3(1.0f);
			body.velocity = Vec3(2.0f);
			container.bodies.pushBack(body);

			container.tinyArray.pushBack(1);
			container.tinyArray.pushBack(2);

			Array<float> af1;
			af1.pushBack(11);
			af1.pushBack(12);
			container.floatArray2D.pushBack(af1);

			Array<float> af2;
			af2.pushBack(21);
			af2.pushBack(22);
			container.floatArray2D.pushBack(af2);

			container.linkList.pushBack(*new MyListNode("I am list node"));

			Type* t = reflection.getType<Container>();
			t->serialize(ose, "My container", &container);
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
			t->serialize(ose, "Basic types", &basicTypes);
		}

		{	Vec3 v(1, 2, 3);
			Type* t = reflection.getType<Vec3>();
			t->serialize(ose, "Vec3 type", &v);
		}

		{	Circle c;
			c.area = 10;
			c.radius = 2;

			Type* t = reflection.getType<Circle>();
			t->serialize(ose, "my circle", &c);
		}

		ose.endArchive();
		char* str = (char*)os.bytePtr();
		CHECK(str);
	}

	// Read
	{
		JsonInputSerializer ise;
		ise.parser.parse((const roUtf8*)os.bytePtr());
		ise.beginArchive();

		{	Container container;
			Type* t = reflection.getType<Container>();
			t->serialize(ise, "My container", &container);
		}

		{	BasicTypes basicTypes;
			Type* t = reflection.getType<BasicTypes>();
			t->serialize(ise, "Basic types", &basicTypes);
		}

		{	Vec3 v;
			Type* t = reflection.getType<Vec3>();
			t->serialize(ise, "Vec3 type", &v);
			CHECK_EQUAL(1, v.x);
			CHECK_EQUAL(2, v.y);
			CHECK_EQUAL(3, v.z);
		}

		{	Circle c;
			Type* t = reflection.getType<Circle>();
			t->serialize(ise, "my circle", &c);

			CHECK_EQUAL(10, c.area);
			CHECK_EQUAL(2, c.radius);
		}

		ise.endArchive();
	}
}
