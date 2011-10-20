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

void rgDriverApplyDefaultState(RgDriverContext* self)
{
	if(!self) return;
	RgDriver* driver = self->driver;
	if(!driver) return;

	driver->useContext(self);

	{	// Give the context a default blend state
		RgDriverBlendState state = {
			0,
			false,
			RgDriverBlendOp_Add, RgDriverBlendOp_Add,
			RgDriverBlendValue_One, RgDriverBlendValue_Zero,
			RgDriverBlendValue_One, RgDriverBlendValue_Zero,
			RgDriverColorWriteMask_EnableAll
		};

		driver->setBlendState(&state);
	}

	{	// Give the context a default depth stencil state
		RgDriverDepthStencilState state = {
			0,
			false, true,
			RgDriverDepthCompareFunc_Less,
			0, -1,
			{	RgDriverDepthCompareFunc_Always,
				RgDriverStencilOp_Keep,
				RgDriverStencilOp_Keep,
				RgDriverStencilOp_Keep
			},
			{	RgDriverDepthCompareFunc_Always,
				RgDriverStencilOp_Keep,
				RgDriverStencilOp_Keep,
				RgDriverStencilOp_Keep
			}
		};

		driver->setDepthStencilState(&state);
		driver->clearDepth(1);
		driver->clearStencil(0);
	}
}
