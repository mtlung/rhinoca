#include "pch.h"
#include "roRenderDriver.h"
#include "../base/roStringHash.h"

using namespace ro;

extern roRDriver* _roNewRenderDriver_GL(const char* driverType, const char* options);
extern roRDriver* _roNewRenderDriver_DX11(const char* driverType, const char* options);

roRDriverContext* roRDriverCurrentContext = NULL;

roRDriver* roNewRenderDriver(const char* driverType, const char* options)
{
	roRDriver* driver = NULL;
	if(stringLowerCaseHash(driverType, 0) == stringHash("gl"))
		driver = _roNewRenderDriver_GL(driverType, options);
	if(stringLowerCaseHash(driverType, 0) == stringHash("dx11"))
		driver = _roNewRenderDriver_DX11(driverType, options);

	if(driver) {
		driver->stallCallback = NULL;
		driver->stallCallbackUserData = NULL;
	}

	return driver;
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

	{	// Give the context a default rasterizer state
		roRDriverRasterizerState state = {
			0,
			false,		// scissorEnable
			false,		// smoothLineEnable
			false,		// multisampleEnable
			false,		// isFrontFaceClockwise
			roRDriverCullMode_Back
		};

		driver->setRasterizerState(&state);
	}

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
