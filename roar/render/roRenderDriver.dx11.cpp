#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roCpuProfiler.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roString.h"
#include "../base/roStringFormat.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"

#include <dxgi.h>
#include <D3Dcompiler.h>
#include <D3DX11async.h>
#include <stddef.h>	// For offsetof
#include <stdio.h>	// For sscanf

// DirectX stuffs
// DX11 tutorial:					http://www.rastertek.com/tutindex.html
// UpdateSubResource vs staging:	http://forums.create.msdn.com/forums/t/47875.aspx
// State Object:					http://msdn.microsoft.com/en-us/library/bb205071.aspx
// DXGI formats:					http://www.directxtutorial.com/TutorialArticles/2-Formats.aspx
// DX10 cbuffer:					http://www.gamedev.net/topic/574711-d3d10-equivalent-of-gluniform/
// DX10 porting:					http://developer.amd.com/.../Riguer-DX10_tips_and_tricks_for_print.pdf
// DX migration:					http://msdn.microsoft.com/en-us/library/windows/desktop/ff476190%28v=vs.85%29.aspx
// DX10 resources usage:			http://msdn.microsoft.com/en-us/library/windows/desktop/bb205127%28v=VS.85%29.aspx
// DX10 FAQ:						http://msdn.microsoft.com/en-us/library/windows/desktop/ee416643%28v=vs.85%29.aspx#constant_buffers
// Ogre impl of buffer:				https://arkeon.dyndns.org/svn-scol/trunk/dependencies/Ogre/Sources/RenderSystems/Direct3D11/src/OgreD3D11HardwareBuffer.cpp

// Table of DX11 usage and CPU access:
/*								Access Flag		UpdateSubresource						Map							Multi-sample
	D3D11_USAGE_DEFAULT				0					Yes								No								No
	D3D11_USAGE_IMMUTABLE 			0					No								No
	D3D11_USAGE_DYNAMIC				Write				No				MAP_WRITE_DISCARD, MAP_WRITE_NO_OVERWRITE
	D3D11_USAGE_STAGING			Read,Write
 */

using namespace ro;

static DefaultAllocator _allocator;

// ----------------------------------------------------------------------
// Common stuffs

static roUint32 _hashAppend(roUint32 hash, roUint32 dataToAppend)
{
	return dataToAppend + (hash << 6) + (hash << 16) - hash;
}

static roUint32 _hash(const void* data, roSize len)
{
	roUint32 h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(roSize i=0; i<len; ++i)
		h = data_[i] + (h << 6) + (h << 16) - h;
	return h;
}

static const char* _shaderTarget[5][3] = {
	{ "vs_5_0", "ps_5_0", "gs_5_0" },
	{ "vs_4_1", "ps_4_1", "gs_4_1" },
	{ "vs_4_0", "ps_4_0", "gs_4_0" },
	{ "vs_4_0_level_9_3", "ps_4_0_level_9_3", "gs_4_0_level_9_3" },
	{ "vs_4_0_level_9_1", "ps_4_0_level_9_1", "gs_4_0_level_9_1" },
};

static const char* _getShaderTarget(ID3D11Device* d3dDevice, roRDriverShaderType type)
{
	roAssert(int(type) < 3);
	if(!d3dDevice) return NULL;
	switch(d3dDevice->GetFeatureLevel()) {
	case D3D_FEATURE_LEVEL_11_0:	return _shaderTarget[0][type];
	case D3D_FEATURE_LEVEL_10_1:	return _shaderTarget[1][type];
	case D3D_FEATURE_LEVEL_10_0:	return _shaderTarget[2][type];
	case D3D_FEATURE_LEVEL_9_3:		return _shaderTarget[3][type];
	default:						return _shaderTarget[4][type];
	}
}

// ----------------------------------------------------------------------
// Context management

// These functions are implemented in platform specific src files, eg. driver2.dx11.windows.cpp
extern roRDriverContext* _newDriverContext_DX11(roRDriver* driver);
extern void _deleteDriverContext_DX11(roRDriverContext* self);
extern bool _initDriverContext_DX11(roRDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_DX11(roRDriverContext* self);
extern roRDriverContext* _getCurrentContext_DX11();

extern void _driverSwapBuffers_DX11();
extern bool _driverChangeResolution_DX11(unsigned width, unsigned height);

extern void rgDriverApplyDefaultState(roRDriverContext* self);

//namespace {

#include "roRenderDriver.dx11.inl"

static void _setViewport(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	D3D11_VIEWPORT viewport;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = zmin;
	viewport.MaxDepth = zmax;
	viewport.TopLeftX = (float)x;
	viewport.TopLeftY = (float)y;
	ctx->dxDeviceContext->RSSetViewports(1, &viewport);
}

static void _setScissorRect(unsigned x, unsigned y, unsigned width, unsigned height)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	D3D10_RECT rect = {
		x, y,
		x + width, y + height
	};

	ctx->dxDeviceContext->RSSetScissorRects(1, &rect);
}

static void _clearColor(float r, float g, float b, float a)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	float color[4] = { r, g, b, a };

	if(ctx->currentRenderTargetViewHash == 0)
		ctx->dxDeviceContext->ClearRenderTargetView(ctx->dxRenderTargetView, color);
	else {
		// Find render target cache
		for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
			if(ctx->renderTargetCache[i].hash != ctx->currentRenderTargetViewHash)
				continue;
			for(unsigned j=0; j<ctx->renderTargetCache[i].rtViews.size(); ++j)
				ctx->dxDeviceContext->ClearRenderTargetView(ctx->renderTargetCache[i].rtViews[j], color);
		}
	}
}

static void _clearDepth(float z)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	if(ctx->currentRenderTargetViewHash == 0)
		ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_DEPTH, z, 0);
	else {
		// Find render target cache
		for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
			if(ctx->renderTargetCache[i].hash != ctx->currentRenderTargetViewHash)
				continue;
			ctx->dxDeviceContext->ClearDepthStencilView(ctx->renderTargetCache[i].depthStencilView, D3D11_CLEAR_DEPTH, z, 0);
		}
	}
}

static void _clearStencil(unsigned char s)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	if(ctx->currentRenderTargetViewHash == 0)
		ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_STENCIL, 1, s);
	else {
		// Find render target cache
		for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
			if(ctx->renderTargetCache[i].hash != ctx->currentRenderTargetViewHash)
				continue;
			ctx->dxDeviceContext->ClearDepthStencilView(ctx->renderTargetCache[i].depthStencilView, D3D11_CLEAR_STENCIL, 1, s);
		}
	}
}

static void _adjustDepthRangeMatrix(float* mat)
{
	// Transform the z from range -1, 1 to 0, 1
	mat[2 + 0*4] = (mat[2 + 0*4] + mat[3 + 0*4]) * 0.5f;
	mat[2 + 1*4] = (mat[2 + 1*4] + mat[3 + 1*4]) * 0.5f;
	mat[2 + 2*4] = (mat[2 + 2*4] + mat[3 + 2*4]) * 0.5f;
	mat[2 + 3*4] = (mat[2 + 3*4] + mat[3 + 3*4]) * 0.5f;
}

typedef struct TextureFormatMapping
{
	roRDriverTextureFormat format;
	unsigned pixelSizeInBytes;
	DXGI_FORMAT dxFormat;
} TextureFormatMapping;

TextureFormatMapping _textureFormatMappings[] = {
	{ roRDriverTextureFormat(0),			0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_RGBA,			4,	DXGI_FORMAT_R8G8B8A8_UNORM },
	{ roRDriverTextureFormat_RGBA_16F,		8,	DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ roRDriverTextureFormat_RGBA_32F,		16,	DXGI_FORMAT_R32G32B32A32_FLOAT },
	{ roRDriverTextureFormat_RGB_16F,		0,	DXGI_FORMAT_UNKNOWN },
	{ roRDriverTextureFormat_RGB_32F,		12,	DXGI_FORMAT_R32G32B32_FLOAT },
	{ roRDriverTextureFormat_L,				1,	DXGI_FORMAT_R8_UNORM },				// NOTE: DX11 no longer support gray scale texture, have to deal with it in shader
	{ roRDriverTextureFormat_A,				1,	DXGI_FORMAT_A8_UNORM },
	{ roRDriverTextureFormat_Depth,			4,	DXGI_FORMAT_D32_FLOAT },
	{ roRDriverTextureFormat_DepthStencil,	4,	DXGI_FORMAT_D24_UNORM_S8_UINT },	// DXGI_FORMAT_D32_FLOAT_S8X24_UINT
	{ roRDriverTextureFormat_PVRTC2,		0,	DXGI_FORMAT_UNKNOWN },
	{ roRDriverTextureFormat_PVRTC4,		0,	DXGI_FORMAT_UNKNOWN },
	{ roRDriverTextureFormat_DXT1,			0,	DXGI_FORMAT_BC1_UNORM },
	{ roRDriverTextureFormat_DXT5,			0,	DXGI_FORMAT_BC3_UNORM },
};

static bool _setRenderTargets(roRDriverTexture** textures, roSize targetCount, bool useDepthStencil)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) false;

	if(targetCount > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)
		return false;

	if(!textures || targetCount == 0) {
		ctx->currentRenderTargetViewHash = 0;
		// Bind default frame buffer
		ctx->dxDeviceContext->OMSetRenderTargets(1, &ctx->dxRenderTargetView.ptr, ctx->dxDepthStencilView);
		return true;
	}

	// Make hash value
	roUint32 hash = _hash(textures, sizeof(*textures) * targetCount);

	if(ctx->currentRenderTargetViewHash == hash)
		return true;

	ctx->currentRenderTargetViewHash = hash;

	// Find render target cache
	for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
		if(ctx->renderTargetCache[i].hash != hash)
			continue;

		// Unbind any texture which is using as shader resource at the moment
		ID3D11ShaderResourceView* views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { NULL };
		ctx->dxDeviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);

		ctx->renderTargetCache[i].lastUsedTime = ctx->lastSwapTime;
		ctx->dxDeviceContext->OMSetRenderTargets(
			num_cast<UINT>(ctx->renderTargetCache[i].rtViews.size()),
			&ctx->renderTargetCache[i].rtViews.typedPtr()->ptr,
			ctx->renderTargetCache[i].depthStencilView.ptr
		);
		return true;
	}

	// Check the dimension of the render targets
	unsigned width = unsigned(-1);
	unsigned height = unsigned(-1);
	for(unsigned i=0; i<targetCount; ++i) {
		unsigned w = textures[i] ? textures[i]->width : ctx->width;
		unsigned h = textures[i] ? textures[i]->height : ctx->height;
		if(width == unsigned(-1)) width = w;
		if(height == unsigned(-1)) height = h;
		if(width != w || height != h) {
			roLog("error", "roRDriver setRenderTargets not all targets having the same dimension\n");
			return false;
		}
	}

	// Create depth and stencil as requested
	if(useDepthStencil) {
		// Search for existing depth and stencil buffers
	}

	// Create render target view for the texture
	RenderTarget renderTarget;
	renderTarget.hash = hash;
	renderTarget.lastUsedTime = ctx->lastSwapTime;

	for(unsigned i=0; i<targetCount; ++i)
	{
		roRDriverTextureImpl* tex = static_cast<roRDriverTextureImpl*>(textures[i]);

		// Create depth stencil view
		if(tex->format == roRDriverTextureFormat_DepthStencil)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC desc = {
				_textureFormatMappings[tex->format].dxFormat,	// Format
				D3D11_DSV_DIMENSION_TEXTURE2D,			// ViewDimension
				0,										// Flags
				{ 0 }
			};
			desc.Texture2D.MipSlice = 0;

			ID3D11DepthStencilView* depthStencilView = NULL;
			HRESULT hr = ctx->dxDevice->CreateDepthStencilView(tex->dxTexture, &desc, &depthStencilView);

			if(FAILED(hr)) {
				roLog("error", "CreateDepthStencilView failed\n");
				return false;
			}

			renderTarget.depthStencilView = depthStencilView;
		}
		// Create color render target view
		else
		{
			ID3D11RenderTargetView* renderTargetView = NULL;
			HRESULT hr = ctx->dxDevice->CreateRenderTargetView(tex->dxTexture, NULL, &renderTargetView);

			if(FAILED(hr)) {
				roLog("error", "CreateRenderTargetView failed\n");
				return false;
			}

			renderTarget.rtViews.pushBack(ComPtr<ID3D11RenderTargetView>(renderTargetView));
		}
	}

	// Unbind any texture which is using as shader resource at the moment
	ID3D11ShaderResourceView* views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { NULL };
	ctx->dxDeviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);


	ctx->renderTargetCache.pushBack(renderTarget);
	ctx->dxDeviceContext->OMSetRenderTargets(
		num_cast<UINT>(renderTarget.rtViews.size()),
		&renderTarget.rtViews.typedPtr()->ptr,
		renderTarget.depthStencilView.ptr
	);

	return true;
}

// ----------------------------------------------------------------------
// State management

static const StaticArray<D3D11_BLEND_OP, 6> _blendOp = {
	D3D11_BLEND_OP(-1),
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX
};

static const StaticArray<D3D11_BLEND, 11> _blendValue = {
	D3D11_BLEND(-1),
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA
};

static const StaticArray<unsigned, 16> _colorWriteMask = {
	0,
	D3D11_COLOR_WRITE_ENABLE_RED,
	D3D11_COLOR_WRITE_ENABLE_GREEN, 0,
	D3D11_COLOR_WRITE_ENABLE_BLUE, 0, 0, 0,
	D3D11_COLOR_WRITE_ENABLE_ALPHA, 0, 0, 0, 0, 0, 0,
	D3D11_COLOR_WRITE_ENABLE_ALL
};

static void _setBlendState(roRDriverBlendState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0)
		state->hash = (void*)_hash(state, sizeof(*state));

	ID3D11BlendState* dxState = NULL;

	if(state->hash == ctx->currentBlendStateHash)
		return;
	else {
		// Try to search for the state in the cache
		for(roSize i=0; i<ctx->blendStateCache.size(); ++i) {
			if(state->hash == ctx->blendStateCache[i].hash) {
				dxState = ctx->blendStateCache[i].dxBlendState;
				ctx->blendStateCache[i].lastUsedTime = ctx->lastSwapTime;
				break;
			}
		}
	}

	// Cache miss, create the state object
	if(!dxState)
	{
		D3D11_BLEND_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;
		desc.RenderTarget[0].BlendEnable = state->enable;
		desc.RenderTarget[0].SrcBlend = _blendValue[state->colorSrc];
		desc.RenderTarget[0].DestBlend = _blendValue[state->colorDst];
		desc.RenderTarget[0].BlendOp = _blendOp[state->colorOp];
		desc.RenderTarget[0].SrcBlendAlpha = _blendValue[state->alphaSrc];
		desc.RenderTarget[0].DestBlendAlpha = _blendValue[state->alphaDst];
		desc.RenderTarget[0].BlendOpAlpha = _blendOp[state->alphaOp];
		desc.RenderTarget[0].RenderTargetWriteMask = num_cast<UINT8>(_colorWriteMask[state->wirteMask]);

		ComPtr<ID3D11BlendState> s;
		HRESULT hr = ctx->dxDevice->CreateBlendState(&desc, &s.ptr);

		if(FAILED(hr)) {
			roLog("error", "CreateBlendState failed\n");
			return;
		}

		BlendState tmp = { state->hash, ctx->lastSwapTime, s };
		ctx->blendStateCache.pushBack(tmp);
		dxState = ctx->blendStateCache.back().dxBlendState;
	}

	ctx->currentBlendStateHash = state->hash;
	ctx->dxDeviceContext->OMSetBlendState(
		dxState,
		0,			// The blend factor, which to use with D3D11_BLEND_BLEND_FACTOR, but we didn't support it yet
		UINT8(-1)	// Bit mask to do with the multi-sample, not used so set all bits to 1
	);
}

static void _setRasterizerState(roRDriverRasterizerState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0)
		state->hash = (void*)_hash(state, sizeof(*state));

	ID3D11RasterizerState* dxState = NULL;

	if(state->hash == ctx->currentRasterizerStateHash)
		return;
	else {
		// Try to search for the state in the cache
		for(roSize i=0; i<ctx->rasterizerState.size(); ++i) {
			if(state->hash == ctx->rasterizerState[i].hash) {
				dxState = ctx->rasterizerState[i].dxRasterizerState;
				ctx->rasterizerState[i].lastUsedTime = ctx->lastSwapTime;
				break;
			}
		}
	}

	// Cache miss, create the state object
	if(!dxState)
	{
		roAssert(roRDriverCullMode_None == D3D11_CULL_NONE);
		roAssert(roRDriverCullMode_Front == D3D11_CULL_FRONT);
		roAssert(roRDriverCullMode_Back == D3D11_CULL_BACK);

		D3D11_RASTERIZER_DESC desc = {
			D3D11_FILL_SOLID,					// Fill mode
			D3D11_CULL_MODE(state->cullMode),	// Cull mode
			!state->isFrontFaceClockwise,		// Is front counter clockwise
			0,									// Depth bias
			0,									// SlopeScaledDepthBias
			0,									// DepthBiasClamp
			true,								// DepthClipEnable
			state->scissorEnable,				// ScissorEnable
			state->multisampleEnable,			// MultisampleEnable
			state->smoothLineEnable				// AntialiasedLineEnable
		};

		ComPtr<ID3D11RasterizerState> s;
		HRESULT hr = ctx->dxDevice->CreateRasterizerState(&desc, &s.ptr);

		if(FAILED(hr)) {
			roLog("error", "CreateRasterizerState failed\n");
			return;
		}

		RasterizerState tmp = { state->hash, ctx->lastSwapTime, s };
		ctx->rasterizerState.pushBack(tmp);
		dxState = ctx->rasterizerState.back().dxRasterizerState;
	}

	ctx->currentRasterizerStateHash = state->hash;
	ctx->dxDeviceContext->RSSetState(dxState);
}

static const StaticArray<D3D11_COMPARISON_FUNC, 9> _comparisonFuncs = {
	D3D11_COMPARISON_FUNC(-1),
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS
};

static const StaticArray<D3D11_STENCIL_OP, 9> _stencilOps = {
	D3D11_STENCIL_OP(-1),
	D3D11_STENCIL_OP_ZERO,
	D3D11_STENCIL_OP_INVERT,
	D3D11_STENCIL_OP_KEEP,
	D3D11_STENCIL_OP_REPLACE,
	D3D11_STENCIL_OP_INCR_SAT,
	D3D11_STENCIL_OP_DECR_SAT,
	D3D11_STENCIL_OP_INCR,
	D3D11_STENCIL_OP_DECR
};

static void _setDepthStencilState(roRDriverDepthStencilState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enableDepthTest,
			sizeof(roRDriverDepthStencilState) - roOffsetof(roRDriverDepthStencilState, roRDriverDepthStencilState::enableDepthTest)
		);
	}

	ID3D11DepthStencilState* dxState = NULL;

	if(state->hash == ctx->currentDepthStencilStateHash)
		return;
	else {
		// Try to search for the state in the cache
		for(roSize i=0; i<ctx->depthStencilStateCache.size(); ++i) {
			if(state->hash == ctx->depthStencilStateCache[i].hash) {
				dxState = ctx->depthStencilStateCache[i].dxDepthStencilState;
				ctx->depthStencilStateCache[i].lastUsedTime = ctx->lastSwapTime;
				break;
			}
		}
	}

	// Cache miss, create the state object
	if(!dxState)
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		// Set up the description of the stencil state.
		desc.DepthEnable = state->enableDepthTest;
		desc.DepthWriteMask = state->enableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = _comparisonFuncs[state->depthFunc];

		desc.StencilEnable = state->enableStencil;
		desc.StencilReadMask =(UINT8)(state->stencilMask);
		desc.StencilWriteMask = (UINT8)(state->stencilMask);

		// Stencil operations if pixel is front-facing.
		desc.FrontFace.StencilFailOp = _stencilOps[state->front.failOp];
		desc.FrontFace.StencilDepthFailOp = _stencilOps[state->front.zFailOp];
		desc.FrontFace.StencilPassOp = _stencilOps[state->front.passOp];
		desc.FrontFace.StencilFunc = _comparisonFuncs[state->front.func];

		// Stencil operations if pixel is back-facing.
		desc.BackFace.StencilFailOp = _stencilOps[state->back.failOp];
		desc.BackFace.StencilDepthFailOp = _stencilOps[state->back.zFailOp];
		desc.BackFace.StencilPassOp = _stencilOps[state->back.passOp];
		desc.BackFace.StencilFunc = _comparisonFuncs[state->back.func];

		ComPtr<ID3D11DepthStencilState> s;
		HRESULT hr = ctx->dxDevice->CreateDepthStencilState(&desc, &s.ptr);

		if(FAILED(hr)) {
			roLog("error", "CreateDepthStencilState failed\n");
			return;
		}

		DepthStencilState tmp = { state->hash, ctx->lastSwapTime, s };
		ctx->depthStencilStateCache.pushBack(tmp);
		dxState = ctx->depthStencilStateCache.back().dxDepthStencilState;
	}

	ctx->currentDepthStencilStateHash = state->hash;
	ctx->dxDeviceContext->OMSetDepthStencilState(dxState, state->stencilRefValue);
}

static const StaticArray<D3D11_FILTER, 5> _textureFilterMode = {
	D3D11_FILTER(-1),
	D3D11_FILTER_MIN_MAG_MIP_POINT,
	D3D11_FILTER_MIN_MAG_MIP_LINEAR,
	D3D11_FILTER_MIN_MAG_MIP_POINT,
	D3D11_FILTER_MIN_MAG_MIP_LINEAR
};

static const StaticArray<D3D11_TEXTURE_ADDRESS_MODE, 5> _textureAddressMode = {
	D3D11_TEXTURE_ADDRESS_MODE(-1),
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_BORDER,
	D3D11_TEXTURE_ADDRESS_MIRROR
};

static void _setTextureState(roRDriverTextureState* states, roSize stateCount, unsigned startingTextureUnit)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !states || stateCount == 0) return;

	for(unsigned i=0; i<stateCount; ++i)
	{
		roRDriverTextureState& state = states[i];

		// Generate the hash value if not yet
		if(state.hash == 0) {
			state.hash = (void*)_hash(
				&state.filter,
				sizeof(roRDriverTextureState) - offsetof(roRDriverTextureState, roRDriverTextureState::filter)
			);
		}

		ID3D11SamplerState* dxState = NULL;
		roSize freeCacheSlot = roSize(-1);

		// Try to search for the state in the cache
		for(roSize j=0; j<ctx->samplerStateCache.size(); ++j) {
			if(state.hash == ctx->samplerStateCache[j].hash) {
				dxState = ctx->samplerStateCache[j].dxSamplerState;
				break;
			}
			if(freeCacheSlot == -1 && !ctx->samplerStateCache[j].dxSamplerState)
				freeCacheSlot = j;
		}

		// Cache miss, create the state object
		if(!dxState) {
			// Not enough cache slot
			if(freeCacheSlot == -1) {
				roLog("error", "roRDriver texture cache slot full\n");
				return;
			}

			D3D11_SAMPLER_DESC samplerDesc = {
				_textureFilterMode[state.filter],
				_textureAddressMode[state.u],
				_textureAddressMode[state.v],
				D3D11_TEXTURE_ADDRESS_CLAMP,
				0,							// Mip-LOD bias
				state.maxAnisotropy + 1,
				D3D11_COMPARISON_NEVER,		// TODO: Understand what is this
				0, 0, 0, 0,					// Border color
				-FLT_MAX, FLT_MAX			// Min/Max LOD
			};

			// Create the texture sampler state.
			ComPtr<ID3D11SamplerState> dxStateComPtr;
			HRESULT hr = ctx->dxDevice->CreateSamplerState(&samplerDesc, &dxStateComPtr.ptr);

			if(FAILED(hr)) {
				roLog("error", "Fail to create sampler state\n");
				continue;
			}

			dxState = dxStateComPtr;
			ctx->samplerStateCache[freeCacheSlot].dxSamplerState = dxStateComPtr;
			ctx->samplerStateCache[freeCacheSlot].hash = state.hash;
		}

		// TODO: Optimize this by gather all dxState in any array and call PSSetSamplers at once
		ctx->dxDeviceContext->PSSetSamplers(startingTextureUnit + i, 1, &dxState);
	}
}

// ----------------------------------------------------------------------
// Buffer

static roRDriverBuffer* _newBuffer()
{
	roRDriverBufferImpl* ret = _allocator.newObj<roRDriverBufferImpl>().unref();
	ret->type = roRDriverBufferType_Vertex;
	ret->usage = roRDriverDataUsage_Static;
	ret->isMapped = false;
	ret->mapUsage = roRDriverMapUsage_Read;
	ret->mapOffset = 0;
	ret->mapSize = 0;
	ret->sizeInBytes = 0;

	ret->hash = 0;
	ret->capacity = 0;
	ret->dxStagingIdx = 0;
	return ret;
}

static void _deleteBuffer(roRDriverBuffer* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return;

	roAssert(!impl->isMapped);

	// Put the DX buffer back into the cache
	if(impl->dxBuffer) {
		BufferCacheEntry tmp = { impl->hash, impl->capacity, ctx->lastSwapTime, impl->dxBuffer };
		ctx->bufferCache.pushBack(tmp);
		impl->dxBuffer = (ID3D11Buffer*)NULL;
	}

	_allocator.deleteObj(static_cast<roRDriverBufferImpl*>(self));
}

static const StaticArray<D3D11_BIND_FLAG, 4> _bufferBindFlag = {
	D3D11_BIND_FLAG(-1),
	D3D11_BIND_VERTEX_BUFFER,
	D3D11_BIND_INDEX_BUFFER,
	D3D11_BIND_CONSTANT_BUFFER
};

static const StaticArray<D3D11_USAGE, 4> _bufferUsage = {
	D3D11_USAGE(-1),
	D3D11_USAGE_IMMUTABLE,
	D3D11_USAGE_DYNAMIC,
	D3D11_USAGE_DEFAULT
};

static bool _updateBuffer(roRDriverBuffer* self, roSize offsetInBytes, const void* data, roSize sizeInBytes);

static bool _initBuffer(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, const void* initData, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	roAssert(!impl->isMapped);
	if(sizeInBytes == 0) initData = NULL;

	UINT cpuAccessFlag =
		(usage == roRDriverDataUsage_Static || usage == roRDriverDataUsage_Dynamic)
		? 0
		: D3D11_CPU_ACCESS_WRITE;

	D3D11_BUFFER_DESC desc = {
		0,						// ByteWidth
		_bufferUsage[usage],	// Usage
		_bufferBindFlag[type],	// BindFlags
		cpuAccessFlag,			// cpuAccessFlag
		0,						// MiscFlags
		0,						// StructureByteStride
	};

	void* hash = (void*)_hash(&desc, sizeof(desc));

	// First check if the DX buffer can be safely reused
	if( impl->hash == hash && impl->capacity >= sizeInBytes &&
		(impl->usage != roRDriverDataUsage_Static)	// If the original buffer is a static one, we can't reuse it anymore
	)
	{
		return _updateBuffer(impl, 0, initData, sizeInBytes);
	}

	// The old DX buffer cannot be reused, put it into the cache
	if(impl->dxBuffer) {
		BufferCacheEntry tmp = { impl->hash, impl->capacity, ctx->lastSwapTime, impl->dxBuffer };
		ctx->bufferCache.pushBack(tmp);
		impl->dxBuffer = (ID3D11Buffer*)NULL;
	}

	impl->type = type;
	impl->usage = usage;
	impl->isMapped = false;
	impl->mapOffset = 0;
	impl->mapSize = 0;
	impl->sizeInBytes = sizeInBytes;
	impl->hash = hash;
	impl->dxStagingIdx = -1;

	// A simple first fit algorithm
	if(impl->usage != roRDriverDataUsage_Static) for(roSize i=0; i<ctx->bufferCache.size(); ++i)
	{
		if(impl->hash != ctx->bufferCache[i].hash)
			continue;
		if(ctx->bufferCache[i].sizeInByte < sizeInBytes)
			continue;

		// Cache hit
		impl->dxBuffer = ctx->bufferCache[i].dxBuffer;
		impl->capacity = ctx->bufferCache[i].sizeInByte;
		ctx->bufferCache.remove(i);

		roIgnoreRet(_updateBuffer(impl, 0, initData, sizeInBytes));
		return true;
	}

	// Cache miss, do create DX buffer
	roAssert(!impl->dxBuffer);

	desc.ByteWidth = num_cast<UINT>(roMaxOf2(sizeInBytes, roSize(1)));	// DirectX doesn't like zero size buffer

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	if(!ctx->dxDevice)
		return false;

	HRESULT hr = ctx->dxDevice->CreateBuffer(&desc, initData ? &data : NULL, &impl->dxBuffer.ptr);

	if(FAILED(hr)) {
		roLog("error", "Fail to create buffer\n");
		return false;
	}

	impl->capacity = sizeInBytes;

	return true;
}

static StagingBuffer* _getStagingBuffer(roRDriverContextImpl* ctx, roSize size, D3D11_MAP mapOption, D3D11_MAPPED_SUBRESOURCE* mapResult)
{
	roAssert(ctx);

	HRESULT hr;
	StagingBuffer* ret = NULL;
	unsigned tryCount = 0;

	while(!ret)
	{
		++tryCount;

		for(roSize i=0; i<ctx->stagingBufferCache.size(); ++i) {
			roSize idx = (i + ctx->stagingBufferCacheSearchIndex) % ctx->stagingBufferCache.size();
			StagingBuffer* sb = &ctx->stagingBufferCache[idx];
			if(sb->size >= size) {
				ret = sb;
				ctx->stagingBufferCacheSearchIndex = idx + 1;
				break;
			}
		}

		// Cache miss, create new one
		if(!ret || tryCount > ctx->stagingBufferCache.size()) {
			D3D11_BUFFER_DESC desc = {
				num_cast<UINT>(size), D3D11_USAGE_STAGING,
				0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0
			};

			ID3D11Buffer* stagingBuffer = NULL;
			hr = ctx->dxDevice->CreateBuffer(&desc, NULL, &stagingBuffer);

			if(FAILED(hr) || !stagingBuffer) {
				roLog("error", "CreateBuffer failed\n");
				roAssert(!stagingBuffer && "Leaking ID3D11Buffer");
				return NULL;
			}

			for(unsigned i=0; i<ctx->stagingBufferCache.size(); ++i) {
				StagingBuffer* sb = &ctx->stagingBufferCache[i];
				if(!sb->dxBuffer) {
					ret = sb;
					break;
				}
			}

			ctx->stagingBufferCache.pushBack();
			ret = &ctx->stagingBufferCache.back();

			ret->size = size;
			roAssert(!ret->mapped);
			ret->lastUsedTime = 0;
			ret->dxBuffer = stagingBuffer;
		}

		// If the staging buffer still in use, keep try to search
		if(ret->mapped) {
			ret = NULL;
			++ctx->stagingBufferCacheSearchIndex;
			continue;
		}

		ret->lastUsedTime = ctx->lastSwapTime;

		if(mapOption == 0)
			break;

		D3D11_MAPPED_SUBRESOURCE mapped = {0};
		hr = ctx->dxDeviceContext->Map(
			ret->dxBuffer,
			0,
			mapOption,
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(mapResult)
			*mapResult = mapped;

		if(FAILED(hr)) {
			// If this staging texture is still busy, keep try to search
			// for another in the cache list, or create a new one.
			if(hr == DXGI_ERROR_WAS_STILL_DRAWING) {
				ret = NULL;
				++ctx->stagingBufferCacheSearchIndex;
				continue;
			}

			roLog("error", "Fail to map staging buffer\n");
			return NULL;
		}

		ret->mapped = true;
	}

	return ret;
}

static const StaticArray<D3D11_MAP, 4> _mapUsage = {
	D3D11_MAP(-1),
	D3D11_MAP_READ,
	D3D11_MAP_WRITE,	// D3D11_MAP_WRITE_DISCARD
	D3D11_MAP_READ_WRITE,
};

static bool _updateBuffer(roRDriverBuffer* self, roSize offsetInBytes, const void* data, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;
	if(impl->isMapped) return false;
	if(offsetInBytes + sizeInBytes > impl->capacity) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

	if(!data || sizeInBytes == 0) return true;

	// Use staging buffer to do the update
	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	if(StagingBuffer* staging = _getStagingBuffer(ctx, sizeInBytes, D3D11_MAP_WRITE, &mapped)) {
		roAssert(mapped.pData);
		roMemcpy(mapped.pData, data, sizeInBytes);
		ctx->dxDeviceContext->Unmap(staging->dxBuffer, 0);
		roAssert(staging->mapped);
		staging->mapped = false;
		D3D11_BOX srcBox = { 0, 0, 0, num_cast<UINT>(sizeInBytes), 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(impl->dxBuffer, 0, num_cast<UINT>(offsetInBytes), 0, 0, staging->dxBuffer, 0, &srcBox);
		return true;
	}

	return false;
}

static void* _mapBuffer(roRDriverBuffer* self, roRDriverMapUsage usage, roSize offsetInBytes, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(impl->isMapped) return NULL;
	if(!impl->dxBuffer) return NULL;

	sizeInBytes = (sizeInBytes == 0) ? impl->sizeInBytes : sizeInBytes;
	if(offsetInBytes + sizeInBytes > impl->sizeInBytes)
		return NULL;

	// Get staging buffer for read/write
	// NOTE: We supply the map usage flag to make sure the staging buffer is ready to use for that purpose
	StagingBuffer* staging = _getStagingBuffer(ctx, sizeInBytes, _mapUsage[usage], NULL);
	if(!staging)
		return NULL;

	roAssert(impl->dxStagingIdx == -1);
	ctx->dxDeviceContext->Unmap(staging->dxBuffer, 0);
	roAssert(staging->mapped);
	staging->mapped = false;

	// Prepare for read
	if(usage & roRDriverMapUsage_Read) {
		D3D11_BOX srcBox = { num_cast<UINT>(offsetInBytes), 0, 0, num_cast<UINT>(offsetInBytes + sizeInBytes), 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(staging->dxBuffer, 0, 0, 0, 0, impl->dxBuffer, 0, &srcBox);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	while(true) {
		HRESULT hr = ctx->dxDeviceContext->Map(
			staging->dxBuffer,
			0,
			_mapUsage[usage],
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(FAILED(hr)) {
			// The buffer is still in use, do something useful on the CPU and try again later
			if(hr == DXGI_ERROR_WAS_STILL_DRAWING) {
				if(ctx->driver->stallCallback)
					(*ctx->driver->stallCallback)(ctx->driver->stallCallbackUserData);
				continue;
			}

			roLog("error", "Fail to map buffer\n");
			return NULL;
		}

		break;	// Map successfully
	}

	staging->mapped = true;
	impl->isMapped = true;
	impl->mapOffset = offsetInBytes;
	impl->mapSize = sizeInBytes;
	impl->mapUsage = usage;
	impl->dxStagingIdx = num_cast<int>(staging - &ctx->stagingBufferCache.front());

	return mapped.pData;
}

static void _unmapBuffer(roRDriverBuffer* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl || !impl->isMapped) return;

	if(!impl->dxBuffer ||  impl->dxStagingIdx < 0)
		return;

	StagingBuffer* staging = &ctx->stagingBufferCache[impl->dxStagingIdx];
	roAssert(staging->dxBuffer);

	ctx->dxDeviceContext->Unmap(staging->dxBuffer, 0);
	roAssert(staging->mapped);
	staging->mapped = false;

	if(impl->mapUsage & roRDriverMapUsage_Write) {
		D3D11_BOX srcBox = { 0, 0, 0, num_cast<UINT>(impl->mapSize), 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(impl->dxBuffer, 0, num_cast<UINT>(impl->mapOffset), 0, 0, staging->dxBuffer, 0, &srcBox);
	}

	impl->dxStagingIdx = -1;
	impl->isMapped = false;
	impl->mapSize = 0;
}

static bool _resizeBuffer(roRDriverBuffer* self, roSize sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return false;
	if(impl->isMapped) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

	roRDriverBuffer* newBuf = _newBuffer();
	if(!_initBuffer(newBuf, impl->type, impl->usage, NULL, sizeInBytes)) return false;

	if(void* mapped = _mapBuffer(self, roRDriverMapUsage_Read, 0, impl->sizeInBytes)) {
		// TODO: May consider ARB_copy_buffer?
		if(!_updateBuffer(newBuf, 0, mapped, impl->sizeInBytes))
			return false;
		_unmapBuffer(self);

		roSwapMemory(impl, newBuf, sizeof(*impl));
		_deleteBuffer(newBuf);
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------
// Texture

static roRDriverTexture* _newTexture()
{
	roRDriverTextureImpl* ret = _allocator.newObj<roRDriverTextureImpl>().unref();
	ret->width = ret->height = 0;
	ret->isMapped = false;
	ret->isYAxisUp = true;
	ret->maxMipLevels = 0;
	ret->mapUsage = roRDriverMapUsage_Read;
	ret->format = roRDriverTextureFormat_Unknown;
	ret->flags = roRDriverTextureFlag_None;

	ret->dxDimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	ret->dxStagingIdx = 0;
	return ret;
}

static void _deleteTexture(roRDriverTexture* self)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return;

	_allocator.deleteObj(static_cast<roRDriverTextureImpl*>(self));
}

static bool _initTexture(roRDriverTexture* self, unsigned width, unsigned height, unsigned maxMipLevels, roRDriverTextureFormat format, roRDriverTextureFlag flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;
	if(impl->format || impl->dxTexture) return false;
	if(width == 0 || height == 0) return false;

	impl->width = width;
	impl->height = height;
	impl->maxMipLevels = maxMipLevels;
	impl->isMapped = false;
	impl->format = format;
	impl->flags = flags;
	impl->dxDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
	impl->dxStagingIdx = -1;

	UINT bindFlags = 0;

	// Setup bind flags
	if(impl->format == roRDriverTextureFormat_DepthStencil)
		bindFlags |= D3D11_BIND_DEPTH_STENCIL;
	else
		bindFlags |= D3D11_BIND_SHADER_RESOURCE;

	if(impl->flags & roRDriverTextureFlag_RenderTarget)
		bindFlags |= D3D11_BIND_RENDER_TARGET;

	// Create dxTexture
	D3D11_TEXTURE2D_DESC desc = {
		impl->width, impl->height,
		impl->maxMipLevels,
		1,			// ArraySize
		_textureFormatMappings[impl->format].dxFormat,
		{ 1, 0 },	// DXGI_SAMPLE_DESC: 1 sample, quality level 0
		D3D11_USAGE_DEFAULT,
		bindFlags,
		0,			// CPUAccessFlags
		0			// MiscFlags
	};

	ID3D11Texture2D* tex2d = NULL;
	HRESULT hr = ctx->dxDevice->CreateTexture2D(&desc, NULL, &tex2d);
	impl->dxTexture = tex2d;

	if(FAILED(hr)) {
		roLog("error", "CreateTexture2D failed\n");
		return false;
	}

	if(bindFlags & D3D11_BIND_SHADER_RESOURCE) {
		ID3D11ShaderResourceView* view = NULL;
		HRESULT hr = ctx->dxDevice->CreateShaderResourceView(impl->dxTexture, NULL, &view);
		impl->dxView = view;

		if(FAILED(hr)) {
			roLog("error", "CreateShaderResourceView failed\n");
			return false;
		}
	}

	return true;
}

static StagingTexture* _getStagingTexture(roRDriverContextImpl* ctx, roRDriverTextureImpl* impl, D3D11_MAP mapOption, D3D11_MAPPED_SUBRESOURCE* mapResult)
{
	roAssert(ctx && impl);

	ID3D11Texture2D* tex2D = static_cast<ID3D11Texture2D*>(impl->dxTexture.ptr);
	if(!tex2D)
		return NULL;

	D3D11_RESOURCE_DIMENSION resourceDimension;
	tex2D->GetType(&resourceDimension);
	if(resourceDimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return NULL;

	D3D11_TEXTURE2D_DESC desc;
	tex2D->GetDesc(&desc);

	HRESULT hr;
	StagingTexture* ret = NULL;
	unsigned hash = _hash(&desc, sizeof(desc));
	unsigned tryCount = 0;

	while(!ret)
	{
		++tryCount;

		for(roSize i=0; i<ctx->stagingTextureCache.size(); ++i) {
			roSize idx = (i + ctx->stagingTextureCacheSearchIndex) % ctx->stagingTextureCache.size();
			StagingTexture* st = &ctx->stagingTextureCache[idx];
			if(st->hash == hash) {
				ret = st;
				ctx->stagingTextureCacheSearchIndex = idx + 1;
				break;
			}
		}

		// Cache miss, create new one
		if(!ret || tryCount > ctx->stagingTextureCache.size()) {
			desc.Usage = D3D11_USAGE_STAGING;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

			ID3D11Texture2D* stagingTexture = NULL;
			hr = ctx->dxDevice->CreateTexture2D(&desc, NULL, &stagingTexture);

			if(FAILED(hr) || !stagingTexture) {
				roLog("error", "CreateTexture2D failed\n");
				roAssert(!stagingTexture && "Leaking ID3D11Texture2D");
				return NULL;
			}

			for(unsigned i=0; i<ctx->stagingTextureCache.size(); ++i) {
				StagingTexture* st = &ctx->stagingTextureCache[i];
				if(!st->dxTexture) {
					ret = st;
					break;
				}
			}

			ctx->stagingTextureCache.pushBack();
			ret = &ctx->stagingTextureCache.back();

			ret->hash = hash;
			roAssert(!ret->mapped);
			ret->lastUsedTime = 0;
			ret->dxTexture = stagingTexture;
		}

		// If the staging buffer still in use, keep try to search
		if(ret->mapped) {
			ret = NULL;
			++ctx->stagingTextureCacheSearchIndex;
			continue;
		}

		ret->lastUsedTime = ctx->lastSwapTime;

		if(mapOption == 0)
			break;

		D3D11_MAPPED_SUBRESOURCE mapped = {0};
		hr = ctx->dxDeviceContext->Map(
			ret->dxTexture,
			0,
			mapOption,
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(mapResult)
			*mapResult = mapped;

		if(FAILED(hr)) {
			// If this staging texture is still busy, keep try to search
			// for another in the cache list, or create a new one.
			if(hr == DXGI_ERROR_WAS_STILL_DRAWING) {
				ret = NULL;
				++ctx->stagingTextureCacheSearchIndex;
				continue;
			}

			roLog("error", "Fail to map staging texture\n");
			return NULL;
		}

		ret->mapped = true;
		roAssert(mapped.RowPitch >= impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes);
	}

	return ret;
}

static bool _updateTexture(roRDriverTexture* self, unsigned mipIndex, unsigned aryIndex, const void* data, roSize rowPaddingInBytes, roSize* bytesRead)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl || !impl->dxTexture) return false;
	if(impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) return false;

	{	// Mip-map check
		D3D11_TEXTURE2D_DESC desc;
		ID3D11Texture2D* tex2D = static_cast<ID3D11Texture2D*>(impl->dxTexture.ptr);
		tex2D->GetDesc(&desc);
		if(mipIndex >= desc.MipLevels)
			roAssert(false && "Updating at specific mip-level not yet supported");
	}

	// Get staging texture for async upload
	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	StagingTexture* staging = _getStagingTexture(
		ctx, impl,
		data ? D3D11_MAP_WRITE : D3D11_MAP(0),
		&mapped
	);

	if(!staging)
		return NULL;

	// Copy the source pixel data to the staging texture
	if(data)
	{
		roAssert(mapped.pData);
		roAssert(mapped.RowPitch >= impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes);

		// Copy the source data row by row
		char* pSrc = (char*)data;
		char* pDst = (char*)mapped.pData;
		const roSize rowSizeInBytes = impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes + rowPaddingInBytes;
		for(unsigned r=0; r<impl->height; ++r, pSrc +=rowSizeInBytes, pDst += mapped.RowPitch)
			memcpy(pDst, pSrc, rowSizeInBytes);

		ctx->dxDeviceContext->Unmap(staging->dxTexture, 0);
		roAssert(staging->mapped);
		staging->mapped = false;

		// Preform async upload using CopySubresourceRegion
		ctx->dxDeviceContext->CopySubresourceRegion(
			impl->dxTexture, mipIndex,
			0, 0, 0,
			staging->dxTexture, 0,
			NULL
		);
	}

	return true;
}

static void* _mapTexture(roRDriverTexture* self, roRDriverMapUsage usage, unsigned mipIndex, unsigned aryIndex, roSize& rowBytes)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;

	if(impl->isMapped) return NULL;
	if(!impl->dxTexture) return NULL;

	// Get staging texture for read/write
	// NOTE: We supply the map usage flag to make sure the staging buffer is ready to use for that purpose
	StagingTexture* staging = _getStagingTexture(ctx, impl, _mapUsage[usage], NULL);
	if(!staging)
		return NULL;

	roAssert(impl->dxStagingIdx == -1);
	ctx->dxDeviceContext->Unmap(staging->dxTexture, 0);
	roAssert(staging->mapped);
	staging->mapped = false;

	// Prepare for read
	if(usage & roRDriverMapUsage_Read)
		ctx->dxDeviceContext->CopyResource(staging->dxTexture, impl->dxTexture);

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	while(true) {
		HRESULT hr = ctx->dxDeviceContext->Map(
			staging->dxTexture,
			mipIndex,
			_mapUsage[usage],
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(FAILED(hr)) {
			// The buffer is still in use, do something useful on the CPU and try again later
			if(hr == DXGI_ERROR_WAS_STILL_DRAWING) {
				if(ctx->driver->stallCallback)
					(*ctx->driver->stallCallback)(ctx->driver->stallCallbackUserData);
				continue;
			}

			roLog("error", "Fail to map texture\n");
			return NULL;
		}

		break;	// Map successfully
	}

	roAssert(mapped.RowPitch >= impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes);
	rowBytes = mapped.RowPitch;

	roAssert(mapped.pData);
	roAssert(!staging->mapped);
	staging->mapped = true;
	impl->isMapped = true;
	impl->mapUsage = usage;
	impl->dxStagingIdx = num_cast<int>(staging - &ctx->stagingTextureCache.front());

	return mapped.pData;
}

static void _unmapTexture(roRDriverTexture* self, unsigned mipIndex, unsigned aryIndex)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl || !impl->isMapped) return;

	if(!impl->dxTexture || impl->dxStagingIdx < 0)
		return;

	StagingTexture* staging = &ctx->stagingTextureCache[impl->dxStagingIdx];
	roAssert(staging->dxTexture);

	ctx->dxDeviceContext->Unmap(staging->dxTexture, mipIndex);
	roAssert(staging->mapped);
	staging->mapped = false;

	if(impl->mapUsage & roRDriverMapUsage_Write)
		ctx->dxDeviceContext->CopyResource(impl->dxTexture, staging->dxTexture);

	impl->dxStagingIdx = -1;
	impl->isMapped = false;
}

static void _generateMipMap(roRDriverTexture* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl->dxView)
		return;

	ctx->dxDeviceContext->GenerateMips(impl->dxView);
}

// ----------------------------------------------------------------------
// Shader

static roRDriverShader* _newShader()
{
	roRDriverShaderImpl* ret = _allocator.newObj<roRDriverShaderImpl>().unref();
	return ret;
}

static void _deleteShader(roRDriverShader* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!ctx || !impl) return;

	for(unsigned i=0; i<ctx->currentShaders.size(); ++i)
		if(ctx->currentShaders[i] == impl) {
			ctx->currentShaders[i] = NULL;
		}

	_allocator.deleteObj(static_cast<roRDriverShaderImpl*>(self));
}

static unsigned _countBits(int v)
{
	unsigned c;
	for(c=0; v; ++c)	// Accumulates the total bits set in v
		v &= v - 1;		// Clear the least significant bit set
	return c;
}

static void _deleteShaderBlob(roByte* blob)
{
	_allocator.free(blob);
}

static bool _initShader(roRDriverShader* self, roRDriverShaderType type, const char** sources, roSize sourceCount, roByte** outBlob, roSize* outBlobSize)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!ctx || !impl || sourceCount == 0) return false;

	if(sourceCount > 1) {
		roLog("error", "DX11 driver only support compiling a single source\n");
		return false;
	}

	// Disallow re-init
	if(impl->dxShader)
		return false;

	ID3D10Blob* shaderBlob = NULL, *errorMessage = NULL;
	const char* shaderProfile = _getShaderTarget(ctx->dxDevice, type);

	HRESULT hr = D3DX11CompileFromMemory(
		sources[0], strlen(sources[0]), NULL,
		NULL,			// D3D10_SHADER_MACRO
		NULL,			// LPD3D10INCLUDE
		"main",			// Entry point
		shaderProfile,	// Shader profile
		D3D10_SHADER_ENABLE_STRICTNESS, 0,
		NULL,			// ID3DX11ThreadPump, for async loading
		&shaderBlob,
		&errorMessage,
		NULL
	);

	if(FAILED(hr)) {
		if(errorMessage) {
			roLog("error", "Shader compilation failed: %s\n", errorMessage->GetBufferPointer());
			errorMessage->Release();
		}
		return false;
	}

	void* p = shaderBlob->GetBufferPointer();
	SIZE_T size = shaderBlob->GetBufferSize();

	if(type == roRDriverShaderType_Vertex)
		hr = ctx->dxDevice->CreateVertexShader(p, size, NULL, (ID3D11VertexShader**)&impl->dxShader);
	else if(type == roRDriverShaderType_Geometry)
		hr = ctx->dxDevice->CreateGeometryShader(p, size, NULL, (ID3D11GeometryShader**)&impl->dxShader);
	else if(type == roRDriverShaderType_Pixel)
		hr = ctx->dxDevice->CreatePixelShader(p, size, NULL, (ID3D11PixelShader**)&impl->dxShader);
	else
		roAssert(false);

	if(FAILED(hr)) {
		roLog("error", "Fail to create shader\n");
		return false;
	}

	// Query the resource binding point
	// See: http://stackoverflow.com/questions/3198904/d3d10-hlsl-how-do-i-bind-a-texture-to-a-global-texture2d-via-reflection
	ComPtr<ID3D11ShaderReflection> reflector;
	D3DReflect(p, size, IID_ID3D11ShaderReflection, (void**)&reflector.ptr);

	D3D11_SHADER_DESC shaderDesc;
	reflector->GetDesc(&shaderDesc);

	for(unsigned i=0; i<shaderDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
		hr = reflector->GetInputParameterDesc(i, &paramDesc);
		if(FAILED(hr))
			break;

		// Combine the semantic index with the semantic name
		String semanticName;
		strFormat(semanticName, "{}{}", paramDesc.SemanticName, paramDesc.SemanticIndex);
		InputParam ip = { stringLowerCaseHash(semanticName.c_str(), 0), _countBits(paramDesc.Mask), paramDesc.ComponentType };
		impl->inputParams.pushBack(ip);

		// For the semantic index == 0, it's a special case that we also consider the semantic name without the index number
		if(paramDesc.SemanticIndex == 0) {
			InputParam ip = { stringLowerCaseHash(paramDesc.SemanticName, 0), _countBits(paramDesc.Mask), paramDesc.ComponentType };
			impl->inputParams.pushBack(ip);
		}
	}

	for(unsigned i=0; i<shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;
		hr = reflector->GetResourceBindingDesc(i, &desc);
		if(FAILED(hr))
			break;

		ShaderResourceBinding cb = { stringHash(desc.Name, 0), desc.BindPoint };
		impl->shaderResourceBindings.pushBack(cb);
	}

	if(outBlob || outBlobSize)
	{
		// Our blob format, which also include some shader reflection information:
		// Size of original buf (4 bytes)
		// Original blob (size bytes)
		// Param count (4 bytes)
		// Param data (4 * 3 bytes each)
		// Resource binding count (4 bytes)
		// Resource binding data (4 * 2 bytes each)
		// TODO: Handle endian problem
		TinyArray<roByte, 128> tmpBuffer;
		roStaticAssert(sizeof(InputParam) == sizeof(roUint32) * 3);

		roUint32 paramCount = num_cast<roUint32>(impl->inputParams.size());
		tmpBuffer.insert(tmpBuffer.size(), (roByte*)&paramCount, sizeof(paramCount));
		tmpBuffer.insert(tmpBuffer.size(), impl->inputParams.bytePtr(), impl->inputParams.sizeInByte());

		roUint32 resourceBindingCount = num_cast<roUint32>(impl->shaderResourceBindings.size());
		tmpBuffer.insert(tmpBuffer.size(), (roByte*)&resourceBindingCount, sizeof(resourceBindingCount));
		tmpBuffer.insert(tmpBuffer.size(), impl->shaderResourceBindings.bytePtr(), impl->shaderResourceBindings.sizeInByte());

		roAssert(outBlob && outBlobSize);
		*outBlobSize = sizeof(roUint32) + size + tmpBuffer.sizeInByte();
		*outBlob = _allocator.malloc(*outBlobSize);

		*((roUint32*)(*outBlob)) = roUint32(size);
		roMemcpy(*outBlob + sizeof(roUint32), p, size);
		roMemcpy(*outBlob + sizeof(roUint32) + size, tmpBuffer.bytePtr(), tmpBuffer.sizeInByte());
	}

	impl->type = type;
	impl->shaderBlob.assign((roByte*)p, size);

	return true;
}

bool _initShaderFromBlob(roRDriverShader* self, roRDriverShaderType type, const roByte* blob, roSize blobSize)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!ctx || !impl || !blob || blobSize == 0) return false;

	HRESULT hr = -1;

	const roByte* blobEnd = blob + blobSize;
	roUint32 originalBlobSize = *((roUint32*)blob);

	blob += sizeof(roUint32);
	if(blob >= blobEnd) return false;

	// Disallow re-init
	if(impl->dxShader)
		return false;

	roByte* originalBlob = (roByte*)blob;
	if(type == roRDriverShaderType_Vertex)
		hr = ctx->dxDevice->CreateVertexShader(originalBlob, originalBlobSize, NULL, (ID3D11VertexShader**)&impl->dxShader);
	else if(type == roRDriverShaderType_Geometry)
		hr = ctx->dxDevice->CreateGeometryShader(originalBlob, originalBlobSize, NULL, (ID3D11GeometryShader**)&impl->dxShader);
	else if(type == roRDriverShaderType_Pixel)
		hr = ctx->dxDevice->CreatePixelShader(originalBlob, originalBlobSize, NULL, (ID3D11PixelShader**)&impl->dxShader);
	else
		roAssert(false);

	if(FAILED(hr)) {
		roLog("error", "Fail to create shader\n");
		return false;
	}

	blob += originalBlobSize;
	if(blob >= blobEnd) return false;

	roUint32 paramCount = *((roUint32*)blob);
	blob += sizeof(roUint32);
	if(blob > blobEnd) return false;

	impl->inputParams.resize(paramCount);
	if(paramCount) {
		roMemcpy(impl->inputParams.bytePtr(), blob, impl->inputParams.sizeInByte());
		blob += impl->inputParams.sizeInByte();
		if(blob >= blobEnd) return false;
	}

	roUint32 resourceBindingCount = *((roUint32*)blob);
	blob += sizeof(roUint32);
	if(blob > blobEnd) return false;

	impl->shaderResourceBindings.resize(resourceBindingCount);
	if(resourceBindingCount) {
		roMemcpy(impl->shaderResourceBindings.bytePtr(), blob, impl->shaderResourceBindings.sizeInByte());
		blob += impl->shaderResourceBindings.sizeInByte();
	}

	if(blob != blobEnd)
		return false;

	impl->type = type;
	impl->shaderBlob.assign(originalBlob, originalBlobSize);

	return true;
}

// If shaders == NULL || shaderCount == 0, this function will unbind all shaders
bool _bindShaders(roRDriverShader** shaders, roSize shaderCount)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return false;

	roRDriverContextImpl::CurrentShaders binded = ctx->currentShaders;	// For detecting redundant state change
	ctx->currentShaders.assign(NULL);

	// Bind used shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		roRDriverShaderImpl* shader = static_cast<roRDriverShaderImpl*>(shaders[i]);
		if(!shader || !shader->dxShader) continue;

		ctx->currentShaders[shader->type] = shader;

		if(binded[shader->type] == shader)	// Avoid redundant state change
			continue;

		if(shader->type == roRDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(static_cast<ID3D11VertexShader*>(shader->dxShader.ptr), NULL, 0);
		else if(shader->type == roRDriverShaderType_Pixel)
			ctx->dxDeviceContext->PSSetShader(static_cast<ID3D11PixelShader*>(shader->dxShader.ptr), NULL, 0);
		else if(shader->type == roRDriverShaderType_Geometry)
			ctx->dxDeviceContext->GSSetShader(static_cast<ID3D11GeometryShader*>(shader->dxShader.ptr), NULL, 0);
		else
			roAssert(false);
	}

	// Unbind any previous shaders (where ctx->currentShaders is still NULL)
	for(unsigned i=0; i<ctx->currentShaders.size(); ++i) {
		if(ctx->currentShaders[i]) continue;

		if(!binded[i])	// Avoid redundant state change
			continue;

		roRDriverShaderType type = roRDriverShaderType(i);
		if(type == roRDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(NULL, NULL, 0);
		else if(type == roRDriverShaderType_Pixel)
			ctx->dxDeviceContext->PSSetShader(NULL, NULL, 0);
		else if(type == roRDriverShaderType_Geometry)
			ctx->dxDeviceContext->GSSetShader(NULL, NULL, 0);
		else
			roAssert(false);
	}

	return true;
}

bool _setUniformBuffer(unsigned nameHash, roRDriverBuffer* buffer, roRDriverShaderBufferInput* input)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return false;

	roRDriverBufferImpl* bufferImpl = static_cast<roRDriverBufferImpl*>(buffer);
	if(!bufferImpl) return false;

	// Offset is not supported in DX11, have to wait until DX11.1.
	// Now we create a tmp buffer to emulate this feature.
	// NOTE: This is a slow path
	roRDriverBufferImpl* tmpBuf = bufferImpl;
	if(input->offset > 0) {
		tmpBuf = static_cast<roRDriverBufferImpl*>(_newBuffer());
		roVerify(_initBuffer(tmpBuf, roRDriverBufferType_Uniform, roRDriverDataUsage_Dynamic, NULL, bufferImpl->sizeInBytes - input->offset));
		D3D11_BOX srcBox = { input->offset, 0, 0, num_cast<UINT>(bufferImpl->sizeInBytes), 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(tmpBuf->dxBuffer, 0, 0, 0, 0, bufferImpl->dxBuffer, 0, &srcBox);
	}

	// Search for the constant buffer with the matching name
	for(unsigned i=0; i<ctx->currentShaders.size(); ++i) {
		roRDriverShaderImpl* shader = ctx->currentShaders[i];
		if(!shader)
			continue;

		for(unsigned j=0; j<shader->shaderResourceBindings.size(); ++j) {
			ShaderResourceBinding& b = shader->shaderResourceBindings[j];
			if(b.nameHash == nameHash) {
				if(shader->type == roRDriverShaderType_Vertex)
					ctx->dxDeviceContext->VSSetConstantBuffers(b.bindPoint, 1, &tmpBuf->dxBuffer.ptr);
				else if(shader->type == roRDriverShaderType_Pixel)
					ctx->dxDeviceContext->PSSetConstantBuffers(b.bindPoint, 1, &tmpBuf->dxBuffer.ptr);
				else if(shader->type == roRDriverShaderType_Geometry)
					ctx->dxDeviceContext->GSSetConstantBuffers(b.bindPoint, 1, &tmpBuf->dxBuffer.ptr);
				break;
			}
		}

		if(input->offset > 0) {
			// The same temp buffer may be used in multiple shader, only do the deletion for the last one
			if(roOccurrence(ctx->constBufferInUse.begin(), ctx->constBufferInUse.end(), ctx->constBufferInUse[shader->type]) == 1)
				_deleteBuffer(ctx->constBufferInUse[shader->type]);
			ctx->constBufferInUse[shader->type] = tmpBuf;
		}
	}

	return true;
}

roStaticAssert(D3D10_REGISTER_COMPONENT_UINT32 == 1);
roStaticAssert(D3D10_REGISTER_COMPONENT_SINT32 == 2);
roStaticAssert(D3D10_REGISTER_COMPONENT_FLOAT32 == 3);
static const StaticArray<DXGI_FORMAT, 20> _inputFormatMapping = {
	DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,	DXGI_FORMAT_UNKNOWN,		DXGI_FORMAT_UNKNOWN,		DXGI_FORMAT_UNKNOWN,			// D3D10_REGISTER_COMPONENT_UNKNOWN
	DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_UINT,	DXGI_FORMAT_R32G32_UINT,	DXGI_FORMAT_R32G32B32_UINT,	DXGI_FORMAT_R32G32B32A32_UINT,	// D3D10_REGISTER_COMPONENT_UINT32
	DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_SINT,	DXGI_FORMAT_R32G32_SINT,	DXGI_FORMAT_R32G32B32_SINT,	DXGI_FORMAT_R32G32B32A32_SINT,	// D3D10_REGISTER_COMPONENT_SINT32
	DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_FLOAT,	DXGI_FORMAT_R32G32_FLOAT,	DXGI_FORMAT_R32G32B32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT,	// D3D10_REGISTER_COMPONENT_FLOAT32
};

bool _bindShaderBuffers(roRDriverShaderBufferInput* inputs, roSize inputCount, unsigned*)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs || inputCount == 0) return false;

	// Make a hash value for the inputs
	unsigned inputHash = 0;
	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		roRDriverShaderBufferInput* i = &inputs[attri];
		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(i->buffer);
		if(!i || !i->buffer) continue;
		if(buffer->type != roRDriverBufferType_Vertex) continue;	// Input layout only consider vertex buffer

		inputHash = _hashAppend(inputHash, i->stride);
		inputHash = _hashAppend(inputHash, i->offset);
	}

	// Search for existing input layout
	InputLayout* inputLayout = NULL;
	bool inputLayoutCacheFound = false;
	for(roSize i=0; i<ctx->inputLayoutCache.size(); ++i)
	{
		if(inputHash == ctx->inputLayoutCache[i].hash) {
			inputLayout = &ctx->inputLayoutCache[i];
			ctx->inputLayoutCache[i].lastUsedTime = ctx->lastSwapTime;
			inputLayoutCacheFound = true;
			break;
		}
	}

	if(!inputLayout) {
		InputLayout tmp;
		tmp.hash = inputHash;
		tmp.lastUsedTime = ctx->lastSwapTime;
		ctx->inputLayoutCache.pushBack(tmp);
		inputLayout = &ctx->inputLayoutCache.back();
	}

	ctx->bindedIndexCount = 0;
	unsigned slotCounter = 0;
	TinyArray<ID3D11Buffer*, 8> vertexBuffers;
	TinyArray<ConstString, 16> semanticNames;
	semanticNames.reserve(inputCount);	// Make sure the continer will never perform allocation anymore

	// Loop for each inputs and do the necessary binding
	for(unsigned i=0; i<inputCount; ++i)
	{
		roRDriverShaderBufferInput* input = &inputs[i];
		if(!input) continue;

		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(input->buffer);
		roRDriverBufferType bufferType = buffer ? buffer->type : roRDriverBufferType_Vertex;

		// Gather information for creating input layout and binding vertex buffer
		if(bufferType == roRDriverBufferType_Vertex)
		{
			// NOTE: The order of the dxBuffer pointers in vertexBuffers is important,
			// luckily how we make inputHash help us guarantee a matching input layout cache
			// will be found for this particular order of dxBuffer supplied.
			vertexBuffers.pushBack(buffer ? buffer->dxBuffer : ComPtr<ID3D11Buffer>());

			if(inputLayoutCacheFound) continue;

			roRDriverShaderImpl* shader = ctx->currentShaders[bufferType];
			for(unsigned j=0; j<ctx->currentShaders.size(); ++j) {
				if(ctx->currentShaders[j] && ctx->currentShaders[j]->type == roRDriverShaderType_Vertex)
					shader = ctx->currentShaders[j];
			}

			if(!shader) {
				roLog("error", "Call bindShaders() before calling bindShaderBuffers()\n");
				return false;
			}

			// Generate nameHash if necessary
			// NOTE: We use lower case hash for semantic name
			if(input->nameHash == 0 && input->name)
				input->nameHash = stringLowerCaseHash(input->name, 0);

			// Search for the input param name
			InputParam* inputParam = NULL;
			for(unsigned j=0; j<shader->inputParams.size(); ++j) {
				if(shader->inputParams[j].nameHash == input->nameHash) {
					inputParam = &shader->inputParams[j];
					break;
				}
			}

			if(!inputParam)
				continue;

			static const unsigned elementSize = 4;	// Vertex data element are always 32 bits
			UINT stride = input->stride == 0 ? inputParam->elementCount * elementSize : input->stride;

			unsigned elementCount = inputParam->elementCount;
			// In case the user supply less data then what requested by the shader
			// For example, in shader we want float4 as vertex position, but we only supply Vec3
			if(stride < elementCount * elementSize)
				elementCount = stride / elementSize;

			// Split the semantic index form the name
			// POSITION1 -> POSITION, 1
			String semanticName = input->name;
			int semanticIndex = 0;
			roVerify(sscanf(input->name, "%[^0-9]%d", semanticName.c_str(), &semanticIndex) > 0);
			semanticNames.pushBack(semanticName.c_str());

			D3D11_INPUT_ELEMENT_DESC inputDesc;
			inputDesc.SemanticName = semanticNames.back().c_str();
			inputDesc.SemanticIndex = semanticIndex;
			inputDesc.Format = _inputFormatMapping[inputParam->type * 5 + elementCount];
			inputDesc.InputSlot = slotCounter++;
			inputDesc.AlignedByteOffset = input->offset;
			inputDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputDesc.InstanceDataStepRate = 0;

			inputLayout->inputDescs.pushBack(inputDesc);

			roAssert(!inputLayout->shaderBlob || inputLayout->shaderBlob == &shader->shaderBlob);
			inputLayout->shaderBlob = &shader->shaderBlob;

			// Info for IASetVertexBuffers
			inputLayout->strides.pushBack(stride);
			inputLayout->offsets.pushBack(0);
		}
		else if(!buffer) {
			continue;
		}
		// Bind uniform buffer
		else if(buffer->type == roRDriverBufferType_Uniform)
		{
			// Generate nameHash if necessary
			if(input->nameHash == 0 && input->name)
				input->nameHash = stringHash(input->name, 0);

			// NOTE: Shader compiler may optimize away the uniform
			roIgnoreRet(_setUniformBuffer(input->nameHash, buffer, input));
		}
		// Bind index buffer
		else if(buffer->type == roRDriverBufferType_Index)
		{
			ctx->bindedIndexCount = buffer->sizeInBytes / sizeof(roUint16);
			ctx->dxDeviceContext->IASetIndexBuffer(buffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
		}
		else {
			roAssert(false && "An unknow buffer was supplied to bindShaderBuffers()");
		}
	}

	// Create input layout
	if(!inputLayoutCacheFound && !inputLayout->inputDescs.isEmpty()) {
		if(!inputLayout->shaderBlob)
			return false;

		HRESULT hr = ctx->dxDevice->CreateInputLayout(
			&inputLayout->inputDescs.front(), num_cast<UINT>(inputLayout->inputDescs.size()),
			inputLayout->shaderBlob->bytePtr(), num_cast<UINT>(inputLayout->shaderBlob->sizeInByte()),
			&inputLayout->layout.ptr
		);

		inputLayout->shaderBlob = NULL;

		if(FAILED(hr)) {
			roLog("error", "Fail to CreateInputLayout\n");
			return false;
		}
	}

	ctx->dxDeviceContext->IASetInputLayout(inputLayout->layout);

	// If more inputs are specified than the shader needed, other valid inputs will be ignored by DirectX
	roAssert(vertexBuffers.size() == inputLayout->strides.size());
	roAssert(vertexBuffers.size() == inputLayout->offsets.size());

	if(vertexBuffers.size() < inputLayout->strides.size())
		return false;

	if(vertexBuffers.isEmpty())
		return false;

	ctx->dxDeviceContext->IASetVertexBuffers(
		0, num_cast<UINT>(inputLayout->inputDescs.size()),
		(ID3D11Buffer**)&vertexBuffers.front(),
		&inputLayout->strides.front(),
		&inputLayout->offsets.front()
	);

	return true;
}

bool _bindShaderTexture(roRDriverShaderTextureInput* inputs, roSize inputCount)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs) return false;

	for(roSize i=0; i<inputCount; ++i)
	{
		roRDriverShaderTextureInput& input = inputs[i];

		roRDriverShaderImpl* shader = static_cast<roRDriverShaderImpl*>(input.shader);
		if(!shader) continue;

		// Generate the hash value if not yet
		if(input.nameHash == 0)
			input.nameHash = stringHash(input.name, 0);

		UINT slot = 0;
		bool slotFound = false;
		for(roSize j=0; j<shader->shaderResourceBindings.size(); ++j) {
			if(shader->shaderResourceBindings[j].nameHash != input.nameHash)
				continue;
			slot = shader->shaderResourceBindings[j].bindPoint;
			slotFound = true;
			break;
		}

		if(!slotFound) {
			roLog("error", "bindShaderTextures() can't find the shader param '%s'!\n", input.name ? input.name : "");
			continue;
		}

		// Set the shader resource to NULL if required
		roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(input.texture);
		if(!impl || impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) {
			ID3D11ShaderResourceView* view = NULL;
			ctx->dxDeviceContext->PSSetShaderResources(slot, 1, &view);
			return true;
		}

		ctx->dxDeviceContext->PSSetShaderResources(slot, 1, &impl->dxView.ptr);
	}

	return true;
}

// ----------------------------------------------------------------------
// Making draw call

static const StaticArray<D3D11_PRIMITIVE_TOPOLOGY, 5> _primitiveTypeMappings = {
	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
};

static void _drawPrimitive(roRDriverPrimitiveType type, roSize offset, roSize vertexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;

	// We emulate triangle fan for DX11
	if(type == roRDriverPrimitiveType_TriangleFan)
	{
		roAssert(vertexCount < TypeOf<roUint16>::valueMax());

		roSize indexCount = (vertexCount - 2) * 3;
		roRDriverBufferImpl* idxBuffer = (roRDriverBufferImpl*)ctx->triangleFanIndexBuffer;

		if(ctx->triangleFanIndexBufferSize < indexCount)
		{
			roVerify(_initBuffer(idxBuffer, roRDriverBufferType_Index, roRDriverDataUsage_Dynamic, NULL, indexCount * sizeof(roUint16)));
			roUint16* index = (roUint16*)_mapBuffer(idxBuffer, roRDriverMapUsage_Write, 0, 0);

			roAssert(index && "Buffer in the buffer pool still not lockable? Seems quite unlikely...");
			if(index) for(roUint16 i=0, count=0; i<indexCount; i+=3, ++count) {
				index[i+0] = 0;
				index[i+1] = count+1;
				index[i+2] = count+2;
			}

			_unmapBuffer(idxBuffer);
			ctx->triangleFanIndexBufferSize = indexCount;
		}

		ctx->dxDeviceContext->IASetIndexBuffer(idxBuffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
		ctx->dxDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->dxDeviceContext->DrawIndexed(num_cast<UINT>(indexCount), 0, num_cast<INT>(offset));
	}
	else {
		ctx->dxDeviceContext->IASetPrimitiveTopology(_primitiveTypeMappings[type]);
		ctx->dxDeviceContext->Draw(num_cast<UINT>(vertexCount), num_cast<UINT>(offset));
	}
}

static void _drawPrimitiveIndexed(roRDriverPrimitiveType type, roSize offset, roSize indexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
	roAssert(indexCount <= ctx->bindedIndexCount);
	ctx->dxDeviceContext->IASetPrimitiveTopology(_primitiveTypeMappings[type]);
	ctx->dxDeviceContext->DrawIndexed(num_cast<UINT>(indexCount), num_cast<UINT>(offset), 0);
}

static void _drawTriangle(roSize offset, roSize vertexCount, unsigned flags)
{
	_drawPrimitive(roRDriverPrimitiveType_TriangleList, offset, vertexCount, flags);
}

static void _drawTriangleIndexed(roSize offset, roSize indexCount, unsigned flags)
{
	_drawPrimitiveIndexed(roRDriverPrimitiveType_TriangleList, offset, indexCount, flags);
}

// ----------------------------------------------------------------------
// Driver

struct roRDriverImpl : public roRDriver
{
	String _driverName;
};	// roRDriver

static void _deleteRenderDriver_DX11(roRDriver* self)
{
	_allocator.deleteObj(static_cast<roRDriverImpl*>(self));
}

//}	// namespace

roRDriver* _roNewRenderDriver_DX11(const char* driverStr, const char*)
{
	roRDriverImpl* ret = _allocator.newObj<roRDriverImpl>().unref();

	ret->destructor = &_deleteRenderDriver_DX11;
	ret->_driverName = driverStr;
	ret->driverName = ret->_driverName.c_str();

	// Setup the function pointers
	ret->newContext = _newDriverContext_DX11;
	ret->deleteContext = _deleteDriverContext_DX11;
	ret->initContext = _initDriverContext_DX11;
	ret->useContext = _useDriverContext_DX11;
	ret->currentContext = _getCurrentContext_DX11;
	ret->swapBuffers = _driverSwapBuffers_DX11;
	ret->changeResolution = _driverChangeResolution_DX11;
	ret->setViewport = _setViewport;
	ret->setScissorRect = _setScissorRect;
	ret->clearColor = _clearColor;
	ret->clearDepth = _clearDepth;
	ret->clearStencil = _clearStencil;

	ret->adjustDepthRangeMatrix = _adjustDepthRangeMatrix;
	ret->setRenderTargets = _setRenderTargets;

	ret->applyDefaultState = rgDriverApplyDefaultState;
	ret->setBlendState = _setBlendState;
	ret->setRasterizerState = _setRasterizerState;
	ret->setDepthStencilState = _setDepthStencilState;
	ret->setTextureState = _setTextureState;

	ret->newBuffer = _newBuffer;
	ret->deleteBuffer = _deleteBuffer;
	ret->initBuffer = _initBuffer;
	ret->updateBuffer = _updateBuffer;
	ret->resizeBuffer = _resizeBuffer;
	ret->mapBuffer = _mapBuffer;
	ret->unmapBuffer = _unmapBuffer;

	ret->newTexture = _newTexture;
	ret->deleteTexture = _deleteTexture;
	ret->initTexture = _initTexture;
	ret->updateTexture = _updateTexture;
	ret->mapTexture = _mapTexture;
	ret->unmapTexture = _unmapTexture;
	ret->generateMipMap = _generateMipMap;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;
	ret->initShaderFromBlob = _initShaderFromBlob;
	ret->deleteShaderBlob = _deleteShaderBlob;

	ret->bindShaders = _bindShaders;
	ret->bindShaderTextures = _bindShaderTexture;
	ret->bindShaderBuffers = _bindShaderBuffers;
//	ret->bindShaderInputCached = _bindShaderInputCached;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;
	ret->drawPrimitive = _drawPrimitive;
	ret->drawPrimitiveIndexed = _drawPrimitiveIndexed;

	return ret;
}
