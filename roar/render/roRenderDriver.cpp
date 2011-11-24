#include "pch.h"
#include "roRenderDriver.h"
#include "../base/roStringHash.h"

using namespace ro;

extern roRDriver* _rgNewRenderDriver_GL(const char* options);
extern roRDriver* _rgNewRenderDriver_DX11(const char* options);

roRDriver* rgNewRenderDriver(const char* driverType, const char* options)
{
	if(stringLowerCaseHash(driverType, 0) == stringHash("gl"))
		return _rgNewRenderDriver_GL(options);
	if(stringLowerCaseHash(driverType, 0) == stringHash("dx11"))
		return _rgNewRenderDriver_DX11(options);
	return NULL;
}

void rgDeleteRenderDriver(roRDriver* self)
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
			0, -1,
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
