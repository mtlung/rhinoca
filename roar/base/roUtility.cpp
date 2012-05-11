#include "pch.h"
#include "roUtility.h"
#include <string.h>

void roMemcpy(void* dst, const void* src, roSize size)
{
	::memcpy(dst, src, size);
}

void roMemmov(void* dst, const void* src, roSize size)
{
	::memmove(dst, src, size);
}

void roZeroMemory(void* dst, roSize size)
{
	::memset(dst, 0, size);
}

void roSwapMemory(void* p1_, void* p2_, roSize size)
{
	char* p1 = (char*)p1_;
	char* e1 = p1 + size;
	char* p2 = (char*)p2_;

	roSize inc = sizeof(roUint64);

	// If p1 and p2 is possible to align together, go the fast path
	if(roPtrInt(p1) % inc == roPtrInt(p2) % inc)
	{
		for(;p1 < e1 && (roPtrInt)(p1) % inc > 0; ++p1, ++p2)
			roSwap(*p1, *p2);

		// The above loop should already ensure p1 and p2 fulfill the alignment requirement
		for(;p1 < e1 - inc; p1+=inc, p2+=inc)
			roSwap((roUint64&)(*p1), (roUint64&)(*p2));

		for(;p1 < e1; ++p1, ++p2)
			roSwap(*p1, *p2);
	}
	// Else we go the slow path, may we still try fast path on system without alignment penalty?
	else {
		for(;p1 < e1; ++p1, ++p2)
			roSwap(*p1, *p2);
	}
}
