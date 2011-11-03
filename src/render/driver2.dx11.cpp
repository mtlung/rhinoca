#include "pch.h"
#include "driver2.h"

#include "../array.h"
#include "../common.h"
#include "../rhlog.h"
#include "../vector.h"
#include "../rhstring.h"

#include <dxgi.h>
#include <D3Dcompiler.h>
#include <D3DX11async.h>

// DirectX stuffs
// State Object:					http://msdn.microsoft.com/en-us/library/bb205071.aspx
// DX migration:					http://msdn.microsoft.com/en-us/library/windows/desktop/ff476190%28v=vs.85%29.aspx
// DX11 tutorial:					http://www.rastertek.com/tutindex.html
// DX10 porting:					http://developer.amd.com/.../Riguer-DX10_tips_and_tricks_for_print.pdf
// DX10 FAQ:						http://msdn.microsoft.com/en-us/library/windows/desktop/ee416643%28v=vs.85%29.aspx#constant_buffers
// DX10 cbuffer:					http://www.gamedev.net/topic/574711-d3d10-equivalent-of-gluniform/
// DX10 resources usage:			http://msdn.microsoft.com/en-us/library/windows/desktop/bb205127%28v=VS.85%29.aspx
// Ogre impl of buffer:				https://arkeon.dyndns.org/svn-scol/trunk/dependencies/Ogre/Sources/RenderSystems/Direct3D11/src/OgreD3D11HardwareBuffer.cpp
// UpdateSubResource vs staging:	http://forums.create.msdn.com/forums/t/47875.aspx

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

static const char* _getShaderTarget(ID3D11Device* d3dDevice, RgDriverShaderType type)
{
	RHASSERT(int(type) < 3);
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
extern RgDriverContext* _newDriverContext_DX11(RgDriver* driver);
extern void _deleteDriverContext_DX11(RgDriverContext* self);
extern bool _initDriverContext_DX11(RgDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_DX11(RgDriverContext* self);
extern RgDriverContext* _getCurrentContext_DX11();

extern void _driverSwapBuffers_DX11();
extern bool _driverChangeResolution_DX11(unsigned width, unsigned height);

extern void rgDriverApplyDefaultState(RgDriverContext* self);

//namespace {

#include "driver2.dx11.inl"

static void _setViewport(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
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
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	float color[4] = { r, g, b, a };
	ctx->dxDeviceContext->ClearRenderTargetView(ctx->dxRenderTargetView, color);
}

static void _clearDepth(float z)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_DEPTH, z, 0);
}

static void _clearStencil(unsigned char s)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
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

static const Array<D3D11_BLEND_OP, 5> _blendOp = {
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX
};

static const Array<D3D11_BLEND, 10> _blendValue = {
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

static const Array<unsigned, 16> _colorWriteMask = {
	0,
	D3D11_COLOR_WRITE_ENABLE_RED,
	D3D11_COLOR_WRITE_ENABLE_GREEN, 0,
	D3D11_COLOR_WRITE_ENABLE_BLUE, 0, 0, 0,
	D3D11_COLOR_WRITE_ENABLE_ALPHA, 0, 0, 0, 0, 0, 0,
	D3D11_COLOR_WRITE_ENABLE_ALL
};

static void _setBlendState(RgDriverBlendState* state)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enable,
			sizeof(RgDriverBlendState) - offsetof(RgDriverBlendState, RgDriverBlendState::enable)
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
	desc.RenderTarget[0].RenderTargetWriteMask = _colorWriteMask[state->wirteMask];

	ComPtr<ID3D11BlendState> s;
	HRESULT hr = ctx->dxDevice->CreateBlendState(&desc, &s.ptr);

	if(FAILED(hr)) {
		rhLog("error", "CreateBlendState failed\n");
		return;
	}

	ctx->dxDeviceContext->OMSetBlendState(
		s,
		0,	// The blend factor, which to use with D3D11_BLEND_BLEND_FACTOR, but we didn't support it yet
		-1	// Bit mask to do with the multi-sample, not used so set all bits to 1
	);
}

static const Array<D3D11_COMPARISON_FUNC, 8> _comparisonFuncs = {
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS
};

static const Array<D3D11_STENCIL_OP, 8> _stencilOps = {
	D3D11_STENCIL_OP_ZERO,
	D3D11_STENCIL_OP_INVERT,
	D3D11_STENCIL_OP_KEEP,
	D3D11_STENCIL_OP_REPLACE,
	D3D11_STENCIL_OP_INCR,
	D3D11_STENCIL_OP_DECR,
	D3D11_STENCIL_OP_INCR_SAT,
	D3D11_STENCIL_OP_DECR
};

static void _setDepthStencilState(RgDriverDepthStencilState* state)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enableDepth,
			sizeof(RgDriverDepthStencilState) - offsetof(RgDriverDepthStencilState, RgDriverDepthStencilState::enableDepth)
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
	desc.StencilReadMask = static_cast<UINT>(state->stencilMask);
	desc.StencilWriteMask = static_cast<UINT>(state->stencilMask);

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
		rhLog("error", "CreateDepthStencilState failed\n");
		return;
	}

	ctx->dxDeviceContext->OMSetDepthStencilState(s, state->stencilRefValue);
}

static void _setTextureState(RgDriverTextureState* states, unsigned stateCount, unsigned startingTextureUnit)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !states || stateCount == 0) return;
}

//////////////////////////////////////////////////////////////////////////
// Buffer

struct RgDriverBufferImpl : public RgDriverBuffer
{
	ComPtr<ID3D11Buffer> dxBuffer;
	ComPtr<ID3D11Buffer> dxStaging;
};	// RgDriverBufferImpl

static RgDriverBuffer* _newBuffer()
{
	RgDriverBufferImpl* ret = new RgDriverBufferImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(RgDriverBuffer* self)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return;

	RHASSERT(!impl->isMapped);

	delete static_cast<RgDriverBufferImpl*>(self);
}

static const Array<D3D11_BIND_FLAG, 4> _bufferBindFlag = {
	D3D11_BIND_FLAG(-1),
	D3D11_BIND_VERTEX_BUFFER,
	D3D11_BIND_INDEX_BUFFER,
	D3D11_BIND_CONSTANT_BUFFER
};

static const Array<D3D11_USAGE, 4> _bufferUsage = {
	D3D11_USAGE(-1),
	D3D11_USAGE_IMMUTABLE,
	D3D11_USAGE_DYNAMIC,
	D3D11_USAGE_DEFAULT
};

static bool _initBuffer(RgDriverBuffer* self, RgDriverBufferType type, RgDriverDataUsage usage, void* initData, unsigned sizeInBytes)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
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
		(usage == RgDriverDataUsage_Static || desc.Usage == D3D11_USAGE_DEFAULT)
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
		rhLog("error", "Fail to create buffer\n");
		return false;
	}

	return true;
}

static bool _updateBuffer(RgDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(!data) return false;
	if(impl->isMapped) return false;
	if(offsetInBytes + sizeInBytes > self->sizeInBytes) return false;
	if(impl->usage == RgDriverDataUsage_Static) return false;

	// Use staging buffer to do the update
	// TODO: Reduce the dynamic creation/destruction of the staging buffer
	D3D11_BUFFER_DESC stagingBufferDesc = {
		sizeInBytes, D3D11_USAGE_STAGING,
		0, D3D11_CPU_ACCESS_WRITE, 0, 0
	};
	ComPtr<ID3D11Buffer> stagingBuffer;
	D3D11_SUBRESOURCE_DATA dxData = { data, 0, 0 };
	HRESULT hr = ctx->dxDevice->CreateBuffer(&stagingBufferDesc, &dxData, &stagingBuffer.ptr);
	if(FAILED(hr))
		return false;

	ctx->dxDeviceContext->CopySubresourceRegion(impl->dxBuffer.ptr, 0, offsetInBytes, 0, 0, stagingBuffer, 0, NULL);

	return true;
}

static const Array<D3D11_MAP, 5> _mapUsage = {
	D3D11_MAP(-1),
	D3D11_MAP_READ,
	D3D11_MAP_WRITE,
	D3D11_MAP_READ_WRITE,
	D3D11_MAP_WRITE_DISCARD,
};

static void* _mapBuffer(RgDriverBuffer* self, RgDriverBufferMapUsage usage)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(impl->isMapped) return NULL;
	if(!impl->dxBuffer) return NULL;

	// Create staging buffer for read/write
	D3D11_CPU_ACCESS_FLAG flag = D3D11_CPU_ACCESS_FLAG(
		usage & RgDriverBufferMapUsage_Read ? D3D11_CPU_ACCESS_READ : 0 |
		usage & RgDriverBufferMapUsage_Write ? D3D11_CPU_ACCESS_WRITE : 0
	);

	D3D11_BUFFER_DESC stagingBufferDesc = {
		impl->sizeInBytes, D3D11_USAGE_STAGING,
		0, flag, 0, 0
	};

	RHASSERT(!impl->dxStaging);
	HRESULT hr = ctx->dxDevice->CreateBuffer(&stagingBufferDesc, NULL, &impl->dxStaging.ptr);
	if(FAILED(hr) || !impl->dxStaging)
		return NULL;

	// Prepare for read
	if(usage & RgDriverBufferMapUsage_Read)
		ctx->dxDeviceContext->CopyResource(impl->dxStaging, impl->dxBuffer);

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	hr = ctx->dxDeviceContext->Map(
		impl->dxStaging,
		0,
		_mapUsage[usage],
		0,
		&mapped
	);

	if(FAILED(hr)) {
		rhLog("error", "Fail to map buffer\n");
		return NULL;
	}

	impl->isMapped = true;
	impl->mapUsage = usage;

	return mapped.pData;
}

static void _unmapBuffer(RgDriverBuffer* self)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl || !impl->isMapped) return;

	if(!impl->dxBuffer || !impl->dxStaging)
		return;

	ctx->dxDeviceContext->Unmap(impl->dxStaging, 0);

	if(impl->mapUsage & RgDriverBufferMapUsage_Write)
		ctx->dxDeviceContext->CopyResource(impl->dxBuffer, impl->dxStaging);

	impl->dxStaging = (ID3D11Buffer*)NULL;
	impl->isMapped = false;
}

//////////////////////////////////////////////////////////////////////////
// Texture

struct RgDriverTextureImpl : public RgDriverTexture
{
	ComPtr<ID3D11Resource> dxTexture;	// May store a 1d, 2d or 3d texture
	D3D11_RESOURCE_DIMENSION dxDimension;
//	TextureFormatMapping* formatMapping;
};	// RgDriverTextureImpl

static RgDriverTexture* _newTexture()
{
	RgDriverTextureImpl* ret = new RgDriverTextureImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteTexture(RgDriverTexture* self)
{
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!impl) return;

	delete static_cast<RgDriverTextureImpl*>(self);
}

static bool _initTexture(RgDriverTexture* self, unsigned width, unsigned height, RgDriverTextureFormat format)
{
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->dxTexture) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
	impl->dxDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
//	impl->formatMapping = &(_textureFormatMappings[format]);

	return true;
}

typedef struct TextureFormatMapping
{
	RgDriverTextureFormat format;
	unsigned pixelSizeInBytes;
	DXGI_FORMAT dxFormat;
} TextureFormatMapping;

TextureFormatMapping _textureFormatMappings[] = {
	{ RgDriverTextureFormat(0),				0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_RGBA,			4,	DXGI_FORMAT_R8G8B8A8_UINT },
	{ RgDriverTextureFormat_R,				1,	DXGI_FORMAT_R16_UINT },
	{ RgDriverTextureFormat_A,				0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_Depth,			0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_DepthStencil,	0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_PVRTC2,			0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_PVRTC4,			0,	DXGI_FORMAT(0) },
	{ RgDriverTextureFormat_DXT1,			0,	DXGI_FORMAT_BC1_UNORM },
	{ RgDriverTextureFormat_DXT5,			0,	DXGI_FORMAT_BC3_UNORM },
};

static bool _commitTexture(RgDriverTexture* self, const void* data, unsigned rowPaddingInBytes)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;
	if(impl->dxDimension == D3D11_RESOURCE_DIMENSION_UNKNOWN) return false;

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps

	DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };		// 1 sample, quality level 0
	D3D11_TEXTURE2D_DESC desc = {
		impl->width, impl->height,
		mipCount,	// MipLevels
		1,			// ArraySize
		_textureFormatMappings[impl->format].dxFormat,
		sampleDesc,
		D3D11_USAGE_DEFAULT,
		0,	// CPUAccessFlags
		0	// MiscFlags 
	};

	if(rowPaddingInBytes == 0)
		rowPaddingInBytes = impl->width * _textureFormatMappings[impl->format].pixelSizeInBytes;

	D3D11_SUBRESOURCE_DATA dataDesc = {
		data,
		rowPaddingInBytes,
		0	// Only meaningful for 3d texture
	};

	ID3D11Texture2D* tex2d = NULL;
	HRESULT hr = ctx->dxDevice->CreateTexture2D(&desc, data ? &dataDesc : NULL, &tex2d);
	impl->dxTexture = tex2d;

	if(FAILED(hr)) {
		rhLog("error", "CreateTexture2D failed\n");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Shader

static RgDriverShader* _newShader()
{
	RgDriverShaderImpl* ret = new RgDriverShaderImpl;
	return ret;
}

static void _deleteShader(RgDriverShader* self)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
	if(!ctx || !impl) return;

	for(unsigned i=0; i<ctx->currentShaders.size(); ++i)
		if(ctx->currentShaders[i] == impl) {
			ctx->currentShaders[i] = NULL;
		}

	delete static_cast<RgDriverShaderImpl*>(self);
}

static bool _initShader(RgDriverShader* self, RgDriverShaderType type, const char** sources, unsigned sourceCount)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
	if(!ctx || !impl || sourceCount == 0) return false;

	if(sourceCount > 1) {
		rhLog("error", "DX11 driver only support compiling a single source\n");
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
			rhLog("error", "Shader compilation failed: %s\n", errorMessage->GetBufferPointer());
			errorMessage->Release();
		}
		return false;
	}

	self->type = type;

	void* p = shaderBlob->GetBufferPointer();
	SIZE_T size = shaderBlob->GetBufferSize();

	if(type == RgDriverShaderType_Vertex)
		hr = ctx->dxDevice->CreateVertexShader(p, size, NULL, (ID3D11VertexShader**)&impl->dxShader);
	else if(type == RgDriverShaderType_Geometry)
		hr = ctx->dxDevice->CreateGeometryShader(p, size, NULL, (ID3D11GeometryShader**)&impl->dxShader);
	else if(type == RgDriverShaderType_Pixel)
		hr = ctx->dxDevice->CreatePixelShader(p, size, NULL, (ID3D11PixelShader**)&impl->dxShader);
	else
		RHASSERT(false);

	impl->dxShaderBlob = shaderBlob;

	if(FAILED(hr)) {
		rhLog("error", "Fail to create shader\n");
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
		D3D11_SIGNATURE_PARAMETER_DESC paramDesc;	// TODO: Any use of this information?
		hr = reflector->GetInputParameterDesc(i, &paramDesc);
		if(FAILED(hr))
			break;
	}

	for(unsigned i=0; i<shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;
		hr = reflector->GetResourceBindingDesc(i, &desc);
		if(FAILED(hr))
			break;

		ConstantBuffer cb = { StringHash(desc.Name, 0), desc.BindPoint };
		impl->constantBuffers.push_back(cb);
	}

	return true;
}

// If shaders == NULL || shaderCount == 0, this function will unbind all shaders
bool _bindShaders(RgDriverShader** shaders, unsigned shaderCount)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return false;

	ctx->currentShaders.assign(NULL);

	// Bind used shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(shaders[i]);
		if(!shader || !shader->dxShader) continue;

		ctx->currentShaders[shader->type] = shader;
		if(shader->type == RgDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(static_cast<ID3D11VertexShader*>(shader->dxShader.ptr), NULL, 0);
		else if(shader->type == RgDriverShaderType_Pixel)
			ctx->dxDeviceContext->PSSetShader(static_cast<ID3D11PixelShader*>(shader->dxShader.ptr), NULL, 0);
		else if(shader->type == RgDriverShaderType_Geometry)
			ctx->dxDeviceContext->GSSetShader(static_cast<ID3D11GeometryShader*>(shader->dxShader.ptr), NULL, 0);
		else
			RHASSERT(false);
	}

	// Unbind any previous shaders (where ctx->currentShaders is still NULL)
	for(unsigned i=0; i<ctx->currentShaders.size(); ++i) {
		if(ctx->currentShaders[i]) continue;

		RgDriverShaderType type = RgDriverShaderType(i);
		if(type == RgDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(NULL, NULL, 0);
		else if(type == RgDriverShaderType_Pixel)
			ctx->dxDeviceContext->PSSetShader(NULL, NULL, 0);
		else if(type == RgDriverShaderType_Geometry)
			ctx->dxDeviceContext->GSSetShader(NULL, NULL, 0);		
		else
			RHASSERT(false);
	}

	return true;
}

bool _setUniformBuffer(unsigned nameHash, RgDriverBuffer* buffer, RgDriverShaderInput* input)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return false;

	RgDriverBufferImpl* bufferImpl = static_cast<RgDriverBufferImpl*>(buffer);
	if(!bufferImpl) return false;

	RgDriverShaderImpl* shader = NULL;
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
	RgDriverBufferImpl* tmpBuf = bufferImpl;
	if(input->offset > 0) {
		tmpBuf = static_cast<RgDriverBufferImpl*>(_newBuffer());
		_initBuffer(tmpBuf, RgDriverBufferType_Uniform, RgDriverDataUsage_Dynamic, NULL, bufferImpl->sizeInBytes - input->offset);
		D3D11_BOX srcBox = { input->offset, 0, 0, bufferImpl->sizeInBytes, 1, 1 };
		ctx->dxDeviceContext->CopySubresourceRegion(tmpBuf->dxBuffer, 0, 0, 0, 0, bufferImpl->dxBuffer, 0, &srcBox);
	}

	if(shader->type == RgDriverShaderType_Vertex)
		ctx->dxDeviceContext->VSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);
	else if(shader->type == RgDriverShaderType_Pixel)
		ctx->dxDeviceContext->PSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);
	else if(shader->type == RgDriverShaderType_Geometry)
		ctx->dxDeviceContext->GSSetConstantBuffers(cb->bindPoint, 1, &tmpBuf->dxBuffer.ptr);

	if(input->offset > 0)
		_deleteBuffer(tmpBuf);

	return true;
}

struct InputLayout
{
	InputLayout() : layout(NULL), shader(NULL) {}
	ComPtr<ID3D11InputLayout> layout;
	RgDriverShaderImpl* shader;
	PreAllocVector<D3D11_INPUT_ELEMENT_DESC, 8> inputDescs;

	PreAllocVector<ID3D11Buffer*, 8> buffers;
	PreAllocVector<UINT, 8> strides;
	PreAllocVector<UINT, 8> offsets;
};

bool _bindShaderInput(RgDriverShaderInput* inputs, unsigned inputCount, unsigned* cacheId)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs || inputCount == 0) return false;

	InputLayout inputLayout;

	for(unsigned i=0; i<inputCount; ++i)
	{
		RgDriverShaderInput* input = &inputs[i];
		if(!input) continue;

		RgDriverBufferImpl* buffer = static_cast<RgDriverBufferImpl*>(input->buffer);
		if(!buffer) continue;

		// Bind index buffer
		if(buffer->type == RgDriverBufferType_Index)
		{
			ctx->dxDeviceContext->IASetIndexBuffer(buffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
			continue;
		}
		// Gather information for creating input layout and binding vertex buffer
		else if(buffer->type == RgDriverBufferType_Vertex)
		{
			RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(input->shader);
			if(!shader) continue;

			if(!ctx->currentShaders[buffer->type]) {
				rhLog("error", "Call bindShaders() before calling bindShaderInput()\n"); 
				return false;
			}

			D3D11_INPUT_ELEMENT_DESC inputDesc;
			inputDesc.SemanticName = input->name;
			inputDesc.SemanticIndex = 0;
			inputDesc.Format = input->elementCount == 3 ? DXGI_FORMAT_R32G32B32_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;	// TODO: Fix me
			inputDesc.InputSlot = 0;
			inputDesc.AlignedByteOffset = 0;
			inputDesc.InputSlotClass =  D3D11_INPUT_PER_VERTEX_DATA;
			inputDesc.InstanceDataStepRate = 0;

			inputLayout.inputDescs.push_back(inputDesc);
			inputLayout.shader = shader;

			// Info for IASetVertexBuffers
			UINT stride = input->stride == 0 ? input->elementCount * sizeof(float) : input->stride;	// TODO: Fix me
			UINT offset = input->offset;

			inputLayout.strides.push_back(stride);
			inputLayout.offsets.push_back(offset);
			inputLayout.buffers.push_back(buffer->dxBuffer);
		}
		// Bind uniform buffer
		else if(buffer->type == RgDriverBufferType_Uniform)
		{
			// Generate nameHash if necessary
			if(input->nameHash == 0 && input->name)
				input->nameHash = StringHash(input->name);

			if(!_setUniformBuffer(input->nameHash, buffer, input))
				return false;
		}
	}

	// Create input layout
	if(!inputLayout.inputDescs.empty()) {
		ID3D10Blob* shaderBlob = inputLayout.shader->dxShaderBlob;
		if(!shaderBlob)
			return false;

		const unsigned inputCount = inputLayout.inputDescs.size();

		HRESULT hr = ctx->dxDevice->CreateInputLayout(
			&inputLayout.inputDescs.front(), inputCount,
			shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
			&inputLayout.layout.ptr
		);

		if(FAILED(hr)) {
			rhLog("error", "Fail to CreateInputLayout\n");
			return false;
		}

		ctx->dxDeviceContext->IASetInputLayout(inputLayout.layout);

		ctx->dxDeviceContext->IASetVertexBuffers(0, inputCount,
			&inputLayout.buffers.front(),
			&inputLayout.strides.front(),
			&inputLayout.offsets.front()
		);
	}

	return true;
}

bool _setUniformTexture(unsigned nameHash, RgDriverTexture* texture)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Making draw call

static void _drawTriangle(unsigned offset, unsigned vertexCount, unsigned flags)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
}

static void _drawTriangleIndexed(unsigned offset, unsigned indexCount, unsigned flags)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
	ctx->dxDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->dxDeviceContext->DrawIndexed(indexCount, offset, 0);
}

//////////////////////////////////////////////////////////////////////////
// Driver

struct RgDriverImpl : public RgDriver
{
};	// RgDriver

static void _rhDeleteRenderDriver_DX11(RgDriver* self)
{
	delete static_cast<RgDriverImpl*>(self);
}

//}	// namespace

RgDriver* _rgNewRenderDriver_DX11(const char* options)
{
	RgDriverImpl* ret = new RgDriverImpl;
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
