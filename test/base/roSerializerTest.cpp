#include "pch.h"
#include "../../roar/base/roSerializer.h"
#include "../../roar/base/roString.h"

using namespace ro;

TEST(SerializerTest)
{
	ByteArray buf;
	Serializer s;
	s.setBuf(&buf);

	{	int v = 123;
		s.ioRaw(&v, sizeof(v));
		s.io(v);
	}

	{	unsigned v = 123;
		s.ioVary(v);

		v = 256;
		s.ioVary(v);
	}

	{	float v = 4.56f;
		s.io(v);
	}

	{	String v("Hello world!");
		s.io(v);
	}
}
