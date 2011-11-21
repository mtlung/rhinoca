#include "pch.h"
#include <assert.h>

void _roAssert(const wchar_t* expression, const wchar_t* file, unsigned line)
{
	_wassert(expression, file, line);
}
