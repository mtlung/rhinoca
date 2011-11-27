#ifndef __roMemory_h__
#define __roMemory_h__

#include "roBytePtr.h"

roBytePtr	roMalloc(roSize size);
roBytePtr	roRealloc(void* originalPtr, roSize originalSize, roSize newSize);
void		roFree(void* ptr);

struct roDefaultAllocator
{
	roBytePtr malloc(roSize size);
	roBytePtr realloc(void* originalPtr, roSize originalSize, roSize newSize);
	void free(void* ptr);
};

#endif	// __roMemory_h__
