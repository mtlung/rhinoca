#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roString.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"

#include <dxgi.h>
#include <D3Dcompiler.h>
#include <D3DX11async.h>
#include <stddef.h>	// For offsetof

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

static unsigned _hashAppend(unsigned hash, unsigned dataToAppend)
{
	return dataToAppend + (hash << 6) + (hash << 16) - hash; 
}

static unsigned _hash(const void* data, roSize len)
{
	unsigned h = 0;
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
	unsigned hash = _hash(textures, sizeof(*textures) * targetCount);
	ctx->currentRenderTargetViewHash = hash;

	// Find render target cache
	for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
		if(ctx->renderTargetCache[i].hash != hash)
			continue;

		ctx->renderTargetCache[i].lastUsedTime = ctx->lastSwapTime;
		ctx->dxDeviceContext->OMSetRenderTargets(
			num_cast<UINT>(targetCount),
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

	for(unsigned i=0; i<targetCount; ++i) {
		roRDriverTextureImpl* tex = static_cast<roRDriverTextureImpl*>(textures[i]);
		ID3D11RenderTargetView* renderTargetView = NULL;
		HRESULT hr = ctx->dxDevice->CreateRenderTargetView(tex->dxTexture, NULL, &renderTargetView);

		renderTarget.rtViews.pushBack(ComPtr<ID3D11RenderTargetView>(renderTargetView));

		if(FAILED(hr)) {
			roLog("error", "CreateRenderTargetView failed\n");
			return false;
		}
	}

	ctx->renderTargetCache.pushBack(renderTarget);
	ctx->dxDeviceContext->OMSetRenderTargets(
		num_cast<UINT>(targetCount),
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
			&state->enableDepth,
			sizeof(roRDriverDepthStencilState) - roOffsetof(roRDriverDepthStencilState, roRDriverDepthStencilState::enableDepth)
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
		desc.DepthEnable = state->enableDepth;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
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

struct roRDriverBufferImpl : public roRDriverBuffer
{
	void* hash;
	roSize capacity;	// This is the actual size of the dxBuffer
	ComPtr<ID3D11Buffer> dxBuffer;
	StagingBuffer* dxStaging;
};

static roRDriverBuffer* _newBuffer()
{
	roRDriverBufferImpl* ret = _allocator.newObj<roRDriverBufferImpl>().unref();
	memset(ret, 0, sizeof(*ret));
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

static bool _updateBuffer(roRDriverBuffer* self, roSize offsetInBytes, void* data, roSize sizeInBytes);

static bool _initBuffer(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, void* initData, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

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
	if(impl->hash == hash && impl->capacity >= sizeInBytes) {
		roIgnoreRet(_updateBuffer(impl, 0, initData, sizeInBytes));
		return true;
	}

	// The old DX buffer cannot be reused, put it into the cache
	if(impl->dxBuffer) {
		BufferCacheEntry tmp = { impl->hash, impl->capacity, ctx->lastSwapTime, impl->dxBuffer };
		ctx->bufferCache.pushBack(tmp);
		impl->dxBuffer = (ID3D11Buffer*)NULL;
	}

	impl->type = type;
	impl->usage = usage;
	impl->sizeInBytes = sizeInBytes;
	impl->hash = hash;

	// A simple first fit algorithm
	for(roSize i=0; i<ctx->bufferCache.size(); ++i) {
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

	desc.ByteWidth = sizeInBytes;

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = ctx->dxDevice->CreateBuffer(&desc, initData ? &data : NULL, &impl->dxBuffer.ptr);

	if(FAILED(hr)) {
		roLog("error", "Fail to create buffer\n");
		return false;
	}

	impl->capacity = sizeInBytes;

	return true;
}

static StagingBuffer* _getStagingBuffer(roRDriverContextImpl* ctx, void* initData, roSize size)
{
	roAssert(ctx);

	HRESULT hr;
	StagingBuffer* ret = NULL;

	for(unsigned i=0; i<ctx->stagingBufferCache.size(); ++i) {
		StagingBuffer* sb = &ctx->stagingBufferCache[i];
		if(sb->size == size && !sb->busyFrame) {
			ret = sb;
			break;
		}
	}

	if(!ret) {
		// Cache miss, create new one
		D3D11_BUFFER_DESC stagingBufferDesc = {
			num_cast<UINT>(size), D3D11_USAGE_STAGING,
			0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0
		};

		ret = &ctx->stagingBufferCache.pushBack();
		hr = ctx->dxDevice->CreateBuffer(&stagingBufferDesc, NULL, &ret->dxBuffer.ptr);
		if(FAILED(hr) || !ret->dxBuffer)
			return NULL;
	}

	static const unsigned FrameCountForAnyBufferLockFinished = 2;
	ret->size = size;
	ret->mapped = false;
	ret->lastUsedTime = ctx->lastSwapTime;
	ret->busyFrame = FrameCountForAnyBufferLockFinished;

	if(initData) {
		D3D11_MAPPED_SUBRESOURCE mapped = {0};
		hr = ctx->dxDeviceContext->Map(
			ret->dxBuffer,
			0,
			D3D11_MAP_WRITE,
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(FAILED(hr)) {
			roLog("error", "Fail to map buffer\n");
			return NULL;
		}

		memcpy(mapped.pData, initData, size);
		ctx->dxDeviceContext->Unmap(ret->dxBuffer, 0);
	}

	return ret;
}

static const StaticArray<D3D11_MAP, 5> _mapUsage = {
	D3D11_MAP(-1),
	D3D11_MAP_READ,
	D3D11_MAP_WRITE,
	D3D11_MAP_READ_WRITE,
	D3D11_MAP_WRITE_DISCARD,
};

static bool _updateBuffer(roRDriverBuffer* self, roSize offsetInBytes, void* data, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(!data) return false;
	if(impl->isMapped) return false;
	if(offsetInBytes + sizeInBytes > self->sizeInBytes) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

	// Use staging buffer to do the update
	if(StagingBuffer* staging = _getStagingBuffer(ctx, data, sizeInBytes)) {
		ctx->dxDeviceContext->CopySubresourceRegion(impl->dxBuffer.ptr, 0, num_cast<UINT>(offsetInBytes), 0, 0, staging->dxBuffer, 0, NULL);
		return true;
	}

	return false;
}

static void* _mapBuffer(roRDriverBuffer* self, roRDriverBufferMapUsage usage)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(impl->isMapped) return NULL;
	if(!impl->dxBuffer) return NULL;

	// Create staging buffer for read/write
	StagingBuffer* staging = _getStagingBuffer(ctx, NULL, impl->sizeInBytes);
	if(!staging)
		return NULL;

	roAssert(!impl->dxStaging);

	// Prepare for read
	if(usage & roRDriverBufferMapUsage_Read)
		ctx->dxDeviceContext->CopyResource(staging->dxBuffer, impl->dxBuffer);

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	HRESULT hr = ctx->dxDeviceContext->Map(
		staging->dxBuffer,
		0,
		_mapUsage[usage],
		D3D11_MAP_FLAG_DO_NOT_WAIT,
		&mapped
	);

	if(FAILED(hr)) {
		roLog("error", "Fail to map buffer\n");
		return NULL;
	}

	staging->mapped = true;
	impl->isMapped = true;
	impl->mapUsage = usage;
	impl->dxStaging = staging;

	return mapped.pData;
}

static void _unmapBuffer(roRDriverBuffer* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl || !impl->isMapped) return;

	if(!impl->dxBuffer || !impl->dxStaging)
		return;

	roAssert(impl->dxStaging->dxBuffer);

	ctx->dxDeviceContext->Unmap(impl->dxStaging->dxBuffer, 0);

	if(impl->mapUsage & roRDriverBufferMapUsage_Write)
		ctx->dxDeviceContext->CopyResource(impl->dxBuffer, impl->dxStaging->dxBuffer);

	impl->dxStaging->mapped = false;
	impl->dxStaging = NULL;
	impl->isMapped = false;
}

// ----------------------------------------------------------------------
// Texture

static roRDriverTexture* _newTexture()
{
	roRDriverTextureImpl* ret = _allocator.newObj<roRDriverTextureImpl>().unref();
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteTexture(roRDriverTexture* self)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return;

	_allocator.deleteObj(static_cast<roRDriverTextureImpl*>(self));
}

static bool _initTexture(roRDriverTexture* self, unsigned width, unsigned height, roRDriverTextureFormat format, roRDriverTextureFlag flags)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->dxTexture) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
	impl->flags = flags;
	impl->dxDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;

	return true;
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
	{ roRDriverTextureFormat_R,				1,	DXGI_FORMAT_R16_UINT },
	{ roRDriverTextureFormat_A,				0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_Depth,			0,	DXGI_FORMAT_D32_FLOAT },
	{ roRDriverTextureFormat_DepthStencil,	0,	DXGI_FORMAT_D24_UNORM_S8_UINT },	// DXGI_FORMAT_D32_FLOAT_S8X24_UINT
	{ roRDriverTextureFormat_PVRTC2,		0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_PVRTC4,		0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_DXT1,			0,	DXGI_FORMAT_BC1_UNORM },
	{ roRDriverTextureFormat_DXT5,			0,	DXGI_FORMAT_BC3_UNORM },
};

static bool _commitTexture(roRDriverTexture* self, const void* data, roSize rowPaddingInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;
	if(impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) return false;

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps

	UINT bindFlags = 0;

	{	// Setup bind flags
		if(impl->format != roRDriverTextureFormat_Depth && impl->format != roRDriverTextureFormat_DepthStencil)
			bindFlags |= D3D11_BIND_SHADER_RESOURCE;

		if(impl->flags & roRDriverTextureFlag_RenderTarget)
			bindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	D3D11_TEXTURE2D_DESC desc = {
		impl->width, impl->height,
		mipCount,	// MipLevels
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

	// Create staging texture for async upload
	StagingTexture* staging = NULL;
	unsigned hash = _hash(&desc, sizeof(desc));
	unsigned cacheIndex = 0;

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

	// We need to keep try looking at the cache for a free to use cache
	if(data) while(!staging)
	{
		for(; cacheIndex<ctx->stagingTextureCache.size(); ++cacheIndex) {
			StagingTexture* st = &ctx->stagingTextureCache[cacheIndex];
			if(st->hash == hash) {
				staging = st;
				break;
			}
		}

		if(!staging) {
			// Cache miss, create new one
			staging = &ctx->stagingTextureCache.pushBack();

			HRESULT hr = ctx->dxDevice->CreateTexture2D(&desc, NULL, (ID3D11Texture2D**)&staging->dxTexture.ptr);

			if(FAILED(hr)) {
				roLog("error", "CreateTexture2D failed\n");
				return false;
			}
		}

		// Copy the source pixel data to the staging texture
		const roSize rowSizeInBytes = impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes + rowPaddingInBytes;

		D3D11_MAPPED_SUBRESOURCE mapped = {0};
		HRESULT hr = ctx->dxDeviceContext->Map(
			staging->dxTexture,
			0,
			D3D11_MAP_WRITE,
			D3D11_MAP_FLAG_DO_NOT_WAIT,
			&mapped
		);

		if(FAILED(hr)) {
			// If this staging texture is still busy, keep try to search
			// for another in the cache list, or create a new one.
			if(hr == DXGI_ERROR_WAS_STILL_DRAWING) {
				staging = NULL;
				++cacheIndex;
				continue;
			}

			roLog("error", "Fail to map staging texture\n");
			return false;
		}

		roAssert(mapped.RowPitch >= impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes);

		// If we come to here it means that we have a ready to use staging texture
		staging->hash = hash;
		staging->lastUsedTime = ctx->lastSwapTime;

		// Copy the source data row by row
		char* pSrc = (char*)data;
		char* pDst = (char*)mapped.pData;
		for(unsigned r=0; r<impl->height; ++r, pSrc +=rowSizeInBytes, pDst += mapped.RowPitch)
			memcpy(pDst, pSrc, rowSizeInBytes);

		ctx->dxDeviceContext->Unmap(staging->dxTexture, 0);

		// Preform async upload using CopySubresourceRegion
		ctx->dxDeviceContext->CopySubresourceRegion(
			impl->dxTexture, 0,
			0, 0, 0,
			staging->dxTexture, 0,
			NULL
		);

		break;
	}

	if(bindFlags & D3D11_BIND_SHADER_RESOURCE) {
		ID3D11ShaderResourceView* view = NULL;
		hr = ctx->dxDevice->CreateShaderResourceView(impl->dxTexture, NULL, &view);
		impl->dxView = view;

		if(FAILED(hr)) {
			roLog("error", "CreateShaderResourceView failed\n");
			return false;
		}
	}

	return true;
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

static bool _initShader(roRDriverShader* self, roRDriverShaderType type, const char** sources, roSize sourceCount)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!ctx || !impl || sourceCount == 0) return false;

	if(sourceCount > 1) {
		roLog("error", "DX11 driver only support compiling a single source\n");
		return false;
	}

	ID3D10Blob* shaderBlob = NULL, * errorMessage = NULL;
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

	self->type = type;

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

	impl->dxShaderBlob = shaderBlob;

	if(FAILED(hr)) {
		roLog("error", "Fail to create shader\n");
		return false;
	}

	// Query the resource binding point
	// See: http://stackoverflow.com/questions/3198904/d3d10-hlsl-how-do-i-bind-a-texture-to-a-global-texture2d-via-reflection
	ComPtr<ID3D11ShaderReflection> reflector; 
	D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector.ptr);

    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

	for(unsigned i=0; i<shaderDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
		hr = reflector->GetInputParameterDesc(i, &paramDesc);
		if(FAILED(hr))
			break;

		InputParam ip = { stringLowerCaseHash(paramDesc.SemanticName, 0), _countBits(paramDesc.Mask), paramDesc.ComponentType };	// TODO: Fix me
		impl->inputParams.pushBack(ip);
	}

	for(unsigned i=0; i<shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;
		hr = reflector->GetResourceBindingDesc(i, &desc);
		if(FAILED(hr))
			break;

		ConstantBuffer cb = { stringHash(desc.Name, 0), desc.BindPoint };
		impl->constantBuffers.pushBack(cb);
	}

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

bool _setUniformBuffer(unsigned nameHash, roRDriverBuffer* buffer, roRDriverShaderInput* input)
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

	roRDriverShaderImpl* shader = (roRDriverShaderImpl*)input->shader;

	// Search for the constant buffer with the matching name
	if(shader) for(unsigned j=0; j<shader->constantBuffers.size(); ++j) {
		ConstantBuffer& b = shader->constantBuffers[j];
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

	if(input->offset > 0)
		_deleteBuffer(tmpBuf);

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

bool _bindShaderInput(roRDriverShaderInput* inputs, roSize inputCount, unsigned*)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs || inputCount == 0) return false;

	// Make a hash value for the inputs
	unsigned inputHash = 0;
	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		roRDriverShaderInput* i = &inputs[attri];
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

	unsigned slotCounter = 0;
	TinyArray<ID3D11Buffer*, 8> vertexBuffers;

	// Loop for each inputs and do the necessary binding
	for(unsigned i=0; i<inputCount; ++i)
	{
		roRDriverShaderInput* input = &inputs[i];
		if(!input) continue;

		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(input->buffer);
		if(!buffer) continue;

		// Gather information for creating input layout and binding vertex buffer
		if(buffer->type == roRDriverBufferType_Vertex)
		{
			// NOTE: The order of the dxBuffer pointers in vertexBuffers is important,
			// luckily how we make inputHash help us guarantee a matching input layout cache
			// will be found for this particular order of dxBuffer supplied.
			vertexBuffers.pushBack(buffer->dxBuffer);

			if(inputLayoutCacheFound) continue;

			roRDriverShaderImpl* shader = static_cast<roRDriverShaderImpl*>(input->shader);
			if(!shader) continue;

			if(!ctx->currentShaders[buffer->type]) {
				roLog("error", "Call bindShaders() before calling bindShaderInput()\n"); 
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

			D3D11_INPUT_ELEMENT_DESC inputDesc;
			inputDesc.SemanticName = input->name;
			inputDesc.SemanticIndex = 0;
			inputDesc.Format = _inputFormatMapping[inputParam->type * 5 + inputParam->elementCount];
			inputDesc.InputSlot = slotCounter++;
			inputDesc.AlignedByteOffset = input->offset;
			inputDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputDesc.InstanceDataStepRate = 0;

			inputLayout->inputDescs.pushBack(inputDesc);
			inputLayout->shader = shader->dxShaderBlob;

			// Info for IASetVertexBuffers
			UINT stride = input->stride == 0 ? inputParam->elementCount * sizeof(float) : input->stride;

			inputLayout->strides.pushBack(stride);
			inputLayout->offsets.pushBack(0);
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
			ctx->dxDeviceContext->IASetIndexBuffer(buffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
		}
		else {
			roAssert(false && "An unknow buffer was supplied to bindShaderInput()");
		}
	}

	// Create input layout
	if(!inputLayoutCacheFound && !inputLayout->inputDescs.isEmpty()) {
		ID3D10Blob* shaderBlob = inputLayout->shader;
		if(!shaderBlob)
			return false;

		HRESULT hr = ctx->dxDevice->CreateInputLayout(
			&inputLayout->inputDescs.front(), num_cast<UINT>(inputLayout->inputDescs.size()),
			shaderBlob->GetBufferPointer(), num_cast<UINT>(shaderBlob->GetBufferSize()),
			&inputLayout->layout.ptr
		);

		if(FAILED(hr)) {
			roLog("error", "Fail to CreateInputLayout\n");
			return false;
		}
	}

	ctx->dxDeviceContext->IASetInputLayout(inputLayout->layout);

	roAssert(vertexBuffers.size() == inputLayout->strides.size());
	roAssert(vertexBuffers.size() == inputLayout->offsets.size());

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

bool _setUniformTexture(StringHash nameHash, roRDriverTexture* texture)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(texture);
	if(!ctx || !impl) return false;
	if(impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) return false;

	ctx->dxDeviceContext->PSSetShaderResources(0, 1, &impl->dxView.ptr);

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

		unsigned indexCount = (vertexCount - offset - 2) * 3;
		roRDriverBufferImpl* idxBuffer = (roRDriverBufferImpl*)ctx->triangleFanIndexBuffer;

		if(ctx->triangleFanIndexBufferSize < indexCount)
		{
			roVerify(_initBuffer(idxBuffer, roRDriverBufferType_Index, roRDriverDataUsage_Stream, NULL, indexCount * sizeof(roUint16)));
			roUint16* index = (roUint16*)_mapBuffer(idxBuffer, roRDriverBufferMapUsage_Write);

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
		ctx->dxDeviceContext->DrawIndexed(num_cast<UINT>(indexCount), 0, 0);
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

static void _rhDeleteRenderDriver_DX11(roRDriver* self)
{
	_allocator.deleteObj(static_cast<roRDriverImpl*>(self));
}

//}	// namespace

roRDriver* _roNewRenderDriver_DX11(const char* driverStr, const char*)
{
	roRDriverImpl* ret = _allocator.newObj<roRDriverImpl>().unref();
	memset(ret, 0, sizeof(*ret));
	ret->destructor = &_rhDeleteRenderDriver_DX11;
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
	ret->mapBuffer = _mapBuffer;
	ret->unmapBuffer = _unmapBuffer;

	ret->newTexture = _newTexture;
	ret->deleteTexture = _deleteTexture;
	ret->initTexture = _initTexture;
	ret->commitTexture = _commitTexture;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;

	ret->bindShaders = _bindShaders;
	ret->setUniformTexture = _setUniformTexture;

	ret->bindShaderInput = _bindShaderInput;
//	ret->bindShaderInputCached = _bindShaderInputCached;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;
	ret->drawPrimitive = _drawPrimitive;
	ret->drawPrimitiveIndexed = _drawPrimitiveIndexed;

	return ret;
}
