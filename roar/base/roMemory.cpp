#include "pch.h"
#include "roMemory.h"
#include <malloc.h>

roBytePtr roMalloc(roSize size)
{
	return ::malloc(size);
}

roBytePtr roRealloc(void* originalPtr, roSize originalSize, roSize newSize)
{
	// NOTE: Make the behavior of zero new size more explicit
	// Extracts from C99:
	// 7.20.3 (Memory management functions):
	// 1 If the size of the space requested is zero, the behavior is
	//	implementation-defined: either a null pointer is returned, or
	//	the behavior is as if the size were some nonzero value, except
	//	that the returned pointer shall not be used to access an object
	if(!originalPtr && newSize == 0)
		return NULL;

	return ::realloc(originalPtr, newSize);
}

void roFree(void* ptr)
{
	::free(ptr);
}

namespace ro {

roBytePtr DefaultAllocator::malloc(roSize size)
{
	return ::malloc(size);
}

roBytePtr DefaultAllocator::realloc(void* originalPtr, roSize originalSize, roSize newSize)
{
	return ::realloc(originalPtr, newSize);
}

void DefaultAllocator::free(void* ptr)
{
	::free(ptr);
}

}	// namespace ro
