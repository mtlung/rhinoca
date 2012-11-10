#include "pch.h"
#include "roMemory.h"
#include <stdlib.h>
#include "roUtility.h"

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

namespace ro {

roBytePtr DefaultAllocator::malloc(roSize size)
{
	return ::malloc(size);
}

roBytePtr DefaultAllocator::realloc(void* originalPtr, roSize originalSize, roSize newSize)
{
	return roRealloc(originalPtr, originalSize, newSize);
}

void DefaultAllocator::free(void* ptr)
{
	::free(ptr);
}

}	// namespace ro
