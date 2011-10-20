#include "pch.h"
#include "driver2.h"
#include "../rhstring.h"

extern RgDriver* _rgNewRenderDriver_GL(const char* options);
extern RgDriver* _rgNewRenderDriver_DX11(const char* options);

RgDriver* rgNewRenderDriver(const char* driverType, const char* options)
{
	if(StringLowerCaseHash(driverType, 0) == StringHash("gl"))
		return _rgNewRenderDriver_GL(options);
	if(StringLowerCaseHash(driverType, 0) == StringHash("dx11"))
		return _rgNewRenderDriver_DX11(options);
	return NULL;
}

void rgDeleteRenderDriver(RgDriver* self)
{
	if(!self) return;
	(self->destructor)(self);
}
