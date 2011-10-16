#include "pch.h"
#include "driver2.h"
#include "../rhstring.h"

extern RgDriver* _rhNewRenderDriver_GL(const char* options);
extern RgDriver* _rhNewRenderDriver_DX11(const char* options);

RgDriver* rhNewRenderDriver(const char* driverType, const char* options)
{
	if(StringLowerCaseHash(driverType, 0) == StringHash("gl"))
		return _rhNewRenderDriver_GL(options);
	if(StringLowerCaseHash(driverType, 0) == StringHash("dx11"))
		return _rhNewRenderDriver_DX11(options);
	return NULL;
}

void rhDeleteRenderDriver(RgDriver* self)
{
	if(!self) return;
	(self->destructor)(self);
}
