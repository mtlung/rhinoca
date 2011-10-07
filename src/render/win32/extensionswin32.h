#define _(DECLAR, NAME) DECLAR NAME = NULL;
#	include "extensionswin32list.h"
#undef _

static bool glInited = false;

void initExtensions()
{
	if(glInited)
		return;

	glInited = true;

#define GET_FUNC_PTR(type, ptr) \
	if(!(ptr = (type) wglGetProcAddress(#ptr))) \
	{	ptr = (type) wglGetProcAddress(#ptr"EXT");	}

#define _(DECLAR, NAME) GET_FUNC_PTR(DECLAR, NAME);
#	include "extensionswin32list.h"
#undef _

#undef GET_FUNC_PTR
}
