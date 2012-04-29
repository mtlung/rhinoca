#include "pch.h"
#include "roMath.h"
#include "../platform/roCompiler.h"

void roSinCos(float x, float& s, float& c)
{
#if roCOMPILER_VC
	__asm
	{
		fld DWORD PTR[x]
		fsincos
		mov ebx, [c]
		fstp DWORD PTR[ebx]
		mov ebx, [s]
		fstp DWORD PTR[ebx]
	}
#else
	s = roSin(x);
	c = roCos(x);
#endif
}
