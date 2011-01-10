#include "pch.h"
#include "rhinoca.h"
#include <malloc.h>

void* rhinoca_malloc(unsigned int size)
{
	return malloc(size);
}

void rhinoca_free(void* ptr)
{
	free(ptr);
}

void* rhinoca_realloc(void* ptr, rhuint oldSize, rhuint size)
{
	return realloc(ptr, size);
}
