#include "pch.h"
#include "../../roar/base/roSerializer.h"
#include "../../roar/base/roString.h"

using namespace ro;

TEST(SerializerTest)
{
	ByteArray buf;
	Serializer s;
	s.setBuf(&buf);

	{	bool b1 = true;
		bool b2 = false;
		s.io(b1);
		s.io(b2);
	}

	{	int v = 123;
		s.ioRaw(&v, sizeof(v));
		s.io(v);
	}

	{	unsigned v = 123;
		s.ioVary(v);

		v = 256;
		s.ioVary(v);

		v = TypeOf<unsigned>::valueMax();
		s.ioVary(v);
	}

	{	float v = 4.56f;
		s.io(v);
	}

	{	double v = 7.89;
		s.io(v);
	}

	{	String v("Hello world!");
		s.io(v);
	}

	Deserializer d;
	d.setBuf(&buf);

	{	bool b1, b2;
		d.io(b1);
		d.io(b2);
		CHECK(b1);
		CHECK(!b2);
	}

	{	int v;
		d.ioRaw(&v, sizeof(v));
		CHECK_EQUAL(123, v);
		d.io(v);
		CHECK_EQUAL(123, v);
	}

	{	unsigned v;
		d.ioVary(v);
		CHECK_EQUAL(123u, v);

		d.ioVary(v);
		CHECK_EQUAL(256u, v);

		d.ioVary(v);
		CHECK_EQUAL(TypeOf<unsigned>::valueMax(), v);
	}

	{	float v;
		d.io(v);
		CHECK_EQUAL(4.56f, v);
	}

	{	double v;
		d.io(v);
		CHECK_EQUAL(7.89, v);
	}

	{	String v;
		d.io(v);
	}
}
