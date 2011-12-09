#include "pch.h"
#include "common.h"
#include "rhassert.h"
#include <ctype.h>
#include <math.h>
#include <assert.h>

#ifdef _MSC_VER
void rhAssert(const wchar_t* expression, const wchar_t* file, unsigned line)
{
#ifndef NDEBUG
	_wassert(expression, file, line);
#endif
}
#endif
