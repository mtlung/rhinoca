#include "pch.h"
#include "../../roar/base/roTypeCast.h"

using namespace ro;

class NumericOverflowTest {};

TEST_FIXTURE(NumericOverflowTest, castFromMax)
{
	roInt8 i8 = (1 << 7) - 1;
	roUint8 u8 = (1 << 8) - 1;
	roInt16 i16 = (1 << 15) - 1;
	roUint16 u16 = (1 << 16) - 1;
	float f = ro::TypeOf<float>::valueMax();
	double d = ro::TypeOf<double>::valueMax();

	roStatus ok = roStatus::ok;
	roStatus st = roStatus::numeric_cast_overflow;

	CHECK(ok == roIsValidCast<roInt8>(i8));
	CHECK(st == roIsValidCast<roInt8>(u8));
	CHECK(st == roIsValidCast<roInt8>(i16));
	CHECK(st == roIsValidCast<roInt8>(u16));
	CHECK(st == roIsValidCast<roInt8>(f));
	CHECK(st == roIsValidCast<roInt8>(d));

	CHECK(ok == roIsValidCast<roUint8>(i8));
	CHECK(ok == roIsValidCast<roUint8>(u8));
	CHECK(st == roIsValidCast<roUint8>(i16));
	CHECK(st == roIsValidCast<roUint8>(u16));
	CHECK(st == roIsValidCast<roUint8>(f));
	CHECK(st == roIsValidCast<roUint8>(d));

	CHECK(ok == roIsValidCast<float>(i8));
	CHECK(ok == roIsValidCast<float>(u8));
	CHECK(ok == roIsValidCast<float>(i16));
	CHECK(ok == roIsValidCast<float>(u16));
	CHECK(ok == roIsValidCast<float>(f));
	CHECK(st == roIsValidCast<float>(d));
}

TEST_FIXTURE(NumericOverflowTest, castFromMin)
{
	roInt8 i8 = -128;
	roUint8 u8 = 0;
	roInt16 i16 = -32768;
	roUint16 u16 = 0;
	float f = ro::TypeOf<float>::valueMax();
	double d = ro::TypeOf<double>::valueMax();

	roStatus ok = roStatus::ok;
	roStatus st = roStatus::numeric_cast_overflow;

	CHECK(ok == roIsValidCast<roInt8>(i8));
	CHECK(ok == roIsValidCast<roInt8>(u8));
	CHECK(st == roIsValidCast<roInt8>(i16));
	CHECK(ok == roIsValidCast<roInt8>(u16));
	CHECK(st == roIsValidCast<roInt8>(f));
	CHECK(st == roIsValidCast<roInt8>(d));

	CHECK(st == roIsValidCast<roUint8>(i8));
	CHECK(ok == roIsValidCast<roUint8>(u8));
	CHECK(st == roIsValidCast<roUint8>(i16));
	CHECK(ok == roIsValidCast<roUint8>(u16));
	CHECK(st == roIsValidCast<roUint8>(f));
	CHECK(st == roIsValidCast<roUint8>(d));

	CHECK(ok == roIsValidCast<float>(i8));
	CHECK(ok == roIsValidCast<float>(u8));
	CHECK(ok == roIsValidCast<float>(i16));
	CHECK(ok == roIsValidCast<float>(u16));
	CHECK(ok == roIsValidCast<float>(f));
	CHECK(st == roIsValidCast<float>(d));
}
