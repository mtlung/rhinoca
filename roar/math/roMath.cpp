#include "pch.h"
#include "roMath.h"

void roSinCos(float x, float& s, float& c)
{
	s = roSin(x);
	c = roCos(x);
}
