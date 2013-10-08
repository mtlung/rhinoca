#include "pch.h"
#include "base/roReflection.h"
#include "base/roString.h"
#include "math/roVector.h"
#include "math/roMatrix.h"

namespace ro {

roStatus registerReflection()
{
	using namespace Reflection;

	reflection.Class<bool>("bool");
	reflection.Class<roInt8>("roInt8");
	reflection.Class<roUint8>("roUint8");
	reflection.Class<roInt16>("roInt16");
	reflection.Class<roUint16>("roUint16");
	reflection.Class<roInt32>("roInt32");
	reflection.Class<roUint32>("roUint32");
	reflection.Class<roInt64>("roInt64");
	reflection.Class<roUint64>("roUint64");
	reflection.Class<float>("float");
	reflection.Class<double>("double");
	reflection.Class<char*>("char*");
	reflection.Class<String>("string");

	reflection.Class<Vec2>("Vec2")
		.field("x", &Vec2::x)
		.field("y", &Vec2::y);

	reflection.Class<Vec3>("Vec3")
		.field("x", &Vec3::x)
		.field("y", &Vec3::y)
		.field("z", &Vec3::z);

	reflection.Class<Vec4>("Vec4")
		.field("x", &Vec4::x)
		.field("y", &Vec4::y)
		.field("z", &Vec4::z)
		.field("w", &Vec4::w);

	reflection.Class<Mat4>("Mat4");

	return roStatus::ok;
}

}	// namespace ro
