#include "pch.h"
#include "roRenderDriver.h"
#include "../base/roStringHash.h"

using namespace ro;

extern roRDriver* _roNewRenderDriver_GL(const char* options);
extern roRDriver* _roNewRenderDriver_DX11(const char* options);

roRDriverContext* roRDriverCurrentContext = NULL;

roRDriver* roNewRenderDriver(const char* driverType, const char* options)
{
	if(stringLowerCaseHash(driverType, 0) == stringHash("gl"))
		return _roNewRenderDriver_GL(options);
	if(stringLowerCaseHash(driverType, 0) == stringHash("dx11"))
		return _roNewRenderDriver_DX11(options);
	return NULL;
}

void roDeleteRenderDriver(roRDriver* self)
{
	if(!self) return;
	(self->destructor)(self);
}

void rgDriverApplyDefaultState(roRDriverContext* self)
{
	if(!self) return;
	roRDriver* driver = self->driver;
	if(!driver) return;

	driver->useContext(self);

	{	// Give the context a default blend state
		roRDriverBlendState state = {
			0,
			false,
			roRDriverBlendOp_Add, roRDriverBlendOp_Add,
			roRDriverBlendValue_One, roRDriverBlendValue_Zero,
			roRDriverBlendValue_One, roRDriverBlendValue_Zero,
			roRDriverColorWriteMask_EnableAll
		};

		driver->setBlendState(&state);
	}

	{	// Give the context a default depth stencil state
		roRDriverDepthStencilState state = {
			0,
			false, true,
			roRDriverDepthCompareFunc_Less,
			0, (unsigned short)-1,
			{	roRDriverDepthCompareFunc_Always,
				roRDriverStencilOp_Keep,
				roRDriverStencilOp_Keep,
				roRDriverStencilOp_Keep
			},
			{	roRDriverDepthCompareFunc_Always,
				roRDriverStencilOp_Keep,
				roRDriverStencilOp_Keep,
				roRDriverStencilOp_Keep
			}
		};

		driver->setDepthStencilState(&state);
		driver->clearDepth(1);
		driver->clearStencil(0);
	}
}
