#include "pch.h"
#include "roMemory.h"
#include <malloc.h>

roBytePtr roMalloc(roSize size)
{
	return ::malloc(size);
}

roBytePtr roRealloc(void* originalPtr, roSize originalSize, roSize newSize)
{
	return ::realloc(originalPtr, newSize);
}

void roFree(void* ptr)
{
	::free(ptr);
}

roBytePtr roDefaultAllocator::malloc(roSize size)
{
	return ::malloc(size);
}

roBytePtr roDefaultAllocator::realloc(void* originalPtr, roSize originalSize, roSize newSize)
{
	return ::realloc(originalPtr, newSize);
}

void roDefaultAllocator::free(void* ptr)
{
	::free(ptr);
}
