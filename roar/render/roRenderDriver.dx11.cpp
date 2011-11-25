#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roString.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"

#include <dxgi.h>
#include <D3Dcompiler.h>
#include <D3DX11async.h>

using namespace ro;

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

//////////////////////////////////////////////////////////////////////////
// Common stuffs

static unsigned _hash(const void* data, unsigned len)
{
	unsigned h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(unsigned i=0; i<len; ++i)
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

//////////////////////////////////////////////////////////////////////////
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

static void _clearColor(float r, float g, float b, float a)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	float color[4] = { r, g, b, a };
	ctx->dxDeviceContext->ClearRenderTargetView(ctx->dxRenderTargetView, color);
}

static void _clearDepth(float z)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_DEPTH, z, 0);
}

static void _clearStencil(unsigned char s)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_STENCIL, 1, s);
}

static void _adjustDepthRangeMatrix(float* mat)
{
	// Transform the z from range -1, 1 to 0, 1
	mat[2 + 0*4] = (mat[2 + 0*4] + mat[3 + 0*4]) * 0.5f;
	mat[2 + 1*4] = (mat[2 + 1*4] + mat[3 + 1*4]) * 0.5f;
	mat[2 + 2*4] = (mat[2 + 2*4] + mat[3 + 2*4]) * 0.5f;
	mat[2 + 3*4] = (mat[2 + 3*4] + mat[3 + 3*4]) * 0.5f;
}

//////////////////////////////////////////////////////////////////////////
// State management

static const StaticArray<D3D11_BLEND_OP, 5> _blendOp = {
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX
};

static const StaticArray<D3D11_BLEND, 10> _blendValue = {
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
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enable,
			sizeof(roRDriverBlendState) - roOffsetof(roRDriverBlendState, roRDriverBlendState::enable)
		);
		ctx->currentBlendStateHash = state->hash;
	}
	else if(state->hash == ctx->currentBlendStateHash)
		return;
	else {
		// TODO: Make use of the hash value
	}

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

	ctx->dxDeviceContext->OMSetBlendState(
		s,
		0,			// The blend factor, which to use with D3D11_BLEND_BLEND_FACTOR, but we didn't support it yet
		UINT8(-1)	// Bit mask to do with the multi-sample, not used so set all bits to 1
	);
}

static const StaticArray<D3D11_COMPARISON_FUNC, 8> _comparisonFuncs = {
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS
};

static const StaticArray<D3D11_STENCIL_OP, 8> _stencilOps = {
	D3D11_STENCIL_OP_ZERO,
	D3D11_STENCIL_OP_INVERT,
	D3D11_STENCIL_OP_KEEP,
	D3D11_STENCIL_OP_REPLACE,
	D3D11_STENCIL_OP_INCR,
	D3D11_STENCIL_OP_DECR,
	D3D11_STENCIL_OP_INCR_SAT,
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
		ctx->currentDepthStencilStateHash = state->hash;
	}
	else if(state->hash == ctx->currentDepthStencilStateHash)
		return;
	else {
		// TODO: Make use of the hash value
	}

	D3D11_DEPTH_STENCIL_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	// Set up the description of the stencil state.
	desc.DepthEnable = state->enableDepth;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = _comparisonFuncs[state->depthFunc];

	desc.StencilEnable = state->enableStencil;
	desc.StencilReadMask = num_cast<UINT8>(state->stencilMask);
	desc.StencilWriteMask = num_cast<UINT8>(state->stencilMask);

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

	ctx->dxDeviceContext->OMSetDepthStencilState(s, state->stencilRefValue);
}

static void _setTextureState(roRDriverTextureState* states, unsigned stateCount, unsigned startingTextureUnit)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !states || stateCount == 0) return;
}

//////////////////////////////////////////////////////////////////////////
// Buffer

struct roRDriverBufferImpl : public roRDriverBuffer
{
	ComPtr<ID3D11Buffer> dxBuffer;
	StagingBuffer* dxStaging;
};	// roRDriverBufferImpl

static roRDriverBuffer* _newBuffer()
{
	roRDriverBufferImpl* ret = new roRDriverBufferImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(roRDriverBuffer* self)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return;

	roAssert(!impl->isMapped);

	delete static_cast<roRDriverBufferImpl*>(self);
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

static bool _initBuffer(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, void* initData, unsigned sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	self->type = type;
	self->usage = usage;
	self->sizeInBytes = sizeInBytes;

	D3D11_BIND_FLAG flag = _bufferBindFlag[type];

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = _bufferUsage[usage];
	desc.ByteWidth = sizeInBytes;
	desc.BindFlags = flag;
	desc.CPUAccessFlags = 
		(usage == roRDriverDataUsage_Static || desc.Usage == D3D11_USAGE_DEFAULT)
		? 0
		: D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = ctx->dxDevice->CreateBuffer(&desc, initData ? &data : NULL, &impl->dxBuffer.ptr);

	if(FAILED(hr)) {
		roLog("error", "Fail to create buffer\n");
		return false;
	}

	return true;
}

static StagingBuffer* _getStagingBuffer(roRDriverContextImpl* ctx, void* initData, unsigned size)
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
			size, D3D11_USAGE_STAGING,
			0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0
		};

		ret = &arrayIncSize(ctx->stagingBufferCache);
		hr = ctx->dxDevice->CreateBuffer(&stagingBufferDesc, NULL, &ret->dxBuffer.ptr);
		if(FAILED(hr) || !ret->dxBuffer)
			return NULL;
	}

	static const unsigned FrameCountForAnyBufferLockFinished = 2;
	ret->size = size;
	ret->mapped = false;
	ret->hotness = 1.0f;
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

static bool _updateBuffer(roRDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
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
		ctx->dxDeviceContext->CopySubresourceRegion(impl->dxBuffer.ptr, 0, offsetInBytes, 0, 0, staging->dxBuffer, 0, NULL);
		return true;
	}

	return false;
}

static const StaticArray<D3D11_MAP, 5> _mapUsage = {
	D3D11_MAP(-1),
	D3D11_MAP_READ,
	D3D11_MAP_WRITE,
	D3D11_MAP_READ_WRITE,
	D3D11_MAP_WRITE_DISCARD,
};

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

//////////////////////////////////////////////////////////////////////////
// Texture

struct roRDriverTextureImpl : public roRDriverTexture
{
	ComPtr<ID3D11Resource> dxTexture;	// May store a 1d, 2d or 3d texture
	ComPtr<ID3D11ShaderResourceView> dxView;
	D3D11_RESOURCE_DIMENSION dxDimension;
};	// roRDriverTextureImpl

static roRDriverTexture* _newTexture()
{
	roRDriverTextureImpl* ret = new roRDriverTextureImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteTexture(roRDriverTexture* self)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return;

	delete static_cast<roRDriverTextureImpl*>(self);
}

static bool _initTexture(roRDriverTexture* self, unsigned width, unsigned height, roRDriverTextureFormat format)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->dxTexture) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
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
	{ roRDriverTextureFormat(0),				0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_RGBA,			4,	DXGI_FORMAT_R8G8B8A8_UNORM },
	{ roRDriverTextureFormat_R,				1,	DXGI_FORMAT_R16_UINT },
	{ roRDriverTextureFormat_A,				0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_Depth,			0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_DepthStencil,	0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_PVRTC2,			0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_PVRTC4,			0,	DXGI_FORMAT(0) },
	{ roRDriverTextureFormat_DXT1,			0,	DXGI_FORMAT_BC1_UNORM },
	{ roRDriverTextureFormat_DXT5,			0,	DXGI_FORMAT_BC3_UNORM },
};

static bool _commitTexture(roRDriverTexture* self, const void* data, unsigned rowPaddingInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;
	if(impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) return false;

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps

	D3D11_TEXTURE2D_DESC desc = {
		impl->width, impl->height,
		mipCount,	// MipLevels
		1,			// ArraySize
		_textureFormatMappings[impl->format].dxFormat,
		{ 1, 0 },	// DXGI_SAMPLE_DESC: 1 sample, quality level 0
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
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
			staging = &arrayIncSize(ctx->stagingTextureCache);

			HRESULT hr = ctx->dxDevice->CreateTexture2D(&desc, NULL, (ID3D11Texture2D**)&staging->dxTexture.ptr);

			if(FAILED(hr)) {
				roLog("error", "CreateTexture2D failed\n");
				return false;
			}
		}

		// Copy the source pixel data to the staging texture
		const unsigned rowSizeInBytes = impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes;

		if(rowPaddingInBytes == 0)
			rowPaddingInBytes = rowSizeInBytes;

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

		// If we come to here it means that we have a ready to use staging texture
		staging->hash = hash;
		staging->hotness = 1.0f;

		// Copy the source data row by row
		char* pSrc = (char*)data;
		char* pDst = (char*)mapped.pData;
		for(unsigned r=0; r<impl->height; ++r, pSrc +=rowPaddingInBytes, pDst += rowSizeInBytes)
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

	ID3D11ShaderResourceView* view = NULL;
	hr = ctx->dxDevice->CreateShaderResourceView(impl->dxTexture, NULL, &view);
	impl->dxView = view;

	if(FAILED(hr)) {
		roLog("error", "CreateShaderResourceView failed\n");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Shader

static roRDriverShader* _newShader()
{
	roRDriverShaderImpl* ret = new roRDriverShaderImpl;
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

	delete static_cast<roRDriverShaderImpl*>(self);
}

static unsigned _countBits(int v)
{
	unsigned c;
	for(c=0; v; ++c)	// Accumulates the total bits set in v
		v &= v - 1;		// Clear the least significant bit set
	return c;
}

static bool _initShader(roRDriverShader* self, roRDriverShaderType type, const char** sources, unsigned sourceCount)
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
bool _bindShaders(roRDriverShader** shaders, unsigned shaderCount)
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

	roRDriverShaderImpl* shader = NULL;
	ConstantBuffer* cb = NULL;

	// Search for the constant buffer with the matching name
	for(unsigned i=0; i<ctx->currentShaders.size() && !cb; ++i) {
		shader = ctx->currentShaders[i];
		if(!shader) continue;

		for(unsigned j=0; j<shader->constantBuffers.size(); ++j) {
			ConstantBuffer& b = shader->constantBuffers[j];
			if(b.nameHash == nameHash) {
				cb = &b;
				break;
			}
		}
	}

	if(!shader || !cb) return false;

	// Offset is not supported in DX11, have to wait until DX11.1.
	// Now we create a tmp buffer to emulate this feature.
	// NOTE: This is a slow path
	roRDriverBufferImpl* tmpBuf = bufferImpl;
	if(input->offset > 0) {
		tmpBuf = static_cast<roRDriverBufferImpl*>(_newBuffer());
		_initBuffer(tmpBuf, roRDriverBufferType_Uniform, roRDriverDataUsage_Dynamic, NULL, bufferImpl->sizeInBytes - input->offset);
		D3D11_BOX srcBox = { input->offset, 0, 0, bufferImpl->sizeInBytes, 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(tmpBuf->dxBuffer, 0, 0, 0, 0, bufferImpl->dxBuffer, 0, &srcBox);
	}

	if(shader->type == roRDriverShaderType_Vertex)
		ctx->dxDeviceContext->VSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);
	else if(shader->type == roRDriverShaderType_Pixel)
		ctx->dxDeviceContext->PSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);
	else if(shader->type == roRDriverShaderType_Geometry)
		ctx->dxDeviceContext->GSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);

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

bool _bindShaderInput(roRDriverShaderInput* inputs, unsigned inputCount, unsigned*)
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

		struct BlockToHash {
			ID3D11Buffer* dxBuffer;
			unsigned stride, offset;
		};

		BlockToHash block = {
			buffer->dxBuffer,
			i->stride, i->offset
		};

		inputHash += _hash(&block, sizeof(block));	// We use += such that the order of inputs are not important
	}

	// Search for existing input layout
	InputLayout* inputLayout = NULL;
	bool inputLayoutCacheFound = false;
	for(unsigned i=0; i<ctx->inputLayoutCache.size(); ++i)
	{
		if(inputHash == ctx->inputLayoutCache[i].hash) {
			inputLayout = &ctx->inputLayoutCache[i];
			ctx->inputLayoutCache[i].hotness += 1.0f;
			inputLayoutCacheFound = true;
			break;
		}
	}

	if(!inputLayout) {
		InputLayout tmp;
		tmp.hash = inputHash;
		tmp.hotness = 1.0f;
		ctx->inputLayoutCache.pushBack(tmp);
		inputLayout = &ctx->inputLayoutCache.back();
	}

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
			inputDesc.InputSlot = 0;
			inputDesc.AlignedByteOffset = input->offset;
			inputDesc.InputSlotClass =  D3D11_INPUT_PER_VERTEX_DATA;
			inputDesc.InstanceDataStepRate = 0;

			inputLayout->inputDescs.pushBack(inputDesc);
			inputLayout->shader = shader->dxShaderBlob;

			// Info for IASetVertexBuffers
			UINT stride = input->stride == 0 ? inputParam->elementCount * sizeof(float) : input->stride;

			inputLayout->strides.pushBack(stride);
			inputLayout->offsets.pushBack(0);
			inputLayout->buffers.pushBack(buffer->dxBuffer);
		}
		// Bind uniform buffer
		else if(buffer->type == roRDriverBufferType_Uniform)
		{
			// Generate nameHash if necessary
			if(input->nameHash == 0 && input->name)
				input->nameHash = stringHash(input->name, 0);

			if(!_setUniformBuffer(input->nameHash, buffer, input))
				return false;
		}
		// Bind index buffer
		else if(buffer->type == roRDriverBufferType_Index)
		{
			ctx->dxDeviceContext->IASetIndexBuffer(buffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
		}
		else { roAssert(false); }
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

	ctx->dxDeviceContext->IASetVertexBuffers(
		0, num_cast<UINT>(inputLayout->inputDescs.size()),
		(ID3D11Buffer**)&inputLayout->buffers.front(),
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

	ctx->dxDeviceContext->PSSetShaderResources(0, 1, &impl->dxView.ptr);	// TODO: Fix me
	ID3D11SamplerState* samplerStates = NULL;
	ctx->dxDeviceContext->PSSetSamplers(0, 1, &samplerStates);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Making draw call

static void _drawTriangle(unsigned offset, unsigned vertexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
}

static void _drawTriangleIndexed(unsigned offset, unsigned indexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
	ctx->dxDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->dxDeviceContext->DrawIndexed(indexCount, offset, 0);
}

//////////////////////////////////////////////////////////////////////////
// Driver

struct roRDriverImpl : public roRDriver
{
};	// roRDriver

static void _rhDeleteRenderDriver_DX11(roRDriver* self)
{
	delete static_cast<roRDriverImpl*>(self);
}

//}	// namespace

roRDriver* _rgNewRenderDriver_DX11(const char* options)
{
	roRDriverImpl* ret = new roRDriverImpl;
	memset(ret, 0, sizeof(*ret));
	ret->destructor = &_rhDeleteRenderDriver_DX11;

	// Setup the function pointers
	ret->newContext = _newDriverContext_DX11;
	ret->deleteContext = _deleteDriverContext_DX11;
	ret->initContext = _initDriverContext_DX11;
	ret->useContext = _useDriverContext_DX11;
	ret->currentContext = _getCurrentContext_DX11;
	ret->swapBuffers = _driverSwapBuffers_DX11;
	ret->changeResolution = _driverChangeResolution_DX11;
	ret->setViewport = _setViewport;
	ret->clearColor = _clearColor;
	ret->clearDepth = _clearDepth;
	ret->clearStencil = _clearStencil;

	ret->adjustDepthRangeMatrix = _adjustDepthRangeMatrix;

	ret->applyDefaultState = rgDriverApplyDefaultState;
	ret->setBlendState = _setBlendState;
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

	return ret;
}