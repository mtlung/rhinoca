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
