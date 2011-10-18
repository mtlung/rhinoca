#include "pch.h"
#include "driver2.h"

#include "../array.h"
#include "../common.h"
#include "../rhlog.h"
#include "../vector.h"
#include "../rhstring.h"

#include <dxgi.h>
#include <D3DX11async.h>

// DirectX stuffs
// State Object:	http://msdn.microsoft.com/en-us/library/bb205071.aspx
// DX migration:	http://msdn.microsoft.com/en-us/library/windows/desktop/ff476190%28v=vs.85%29.aspx
// DX11 tutorial:	http://www.rastertek.com/tutindex.html

//////////////////////////////////////////////////////////////////////////
// Common stuffs

template<typename T> void safeRelease(T*& t)
{
	if(t) t->Release();
	t = NULL;
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
extern RgDriverContext* _newDriverContext_DX11();
extern void _deleteDriverContext_DX11(RgDriverContext* self);
extern bool _initDriverContext_DX11(RgDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_DX11(RgDriverContext* self);
extern RgDriverContext* _getCurrentContext_DX11();

extern void _driverSwapBuffers_DX11();
extern bool _driverChangeResolution_DX11(unsigned width, unsigned height);

//namespace {

#include "driver2.dx11.inl"

static void _setViewport(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
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
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	float color[4] = { r, g, b, a };
	ctx->dxDeviceContext->ClearRenderTargetView(ctx->dxRenderTargetView, color);
}

static void _clearDepth(float z)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_DEPTH, z, 0);
}

static void _clearStencil(unsigned char s)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !ctx->dxDeviceContext) return;

	ctx->dxDeviceContext->ClearDepthStencilView(ctx->dxDepthStencilView, D3D11_CLEAR_STENCIL, 1, s);
}

//////////////////////////////////////////////////////////////////////////
// State management

static void _setBlendState(RgDriverBlendState* state)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;
}

static void _setDepthStencilState(RgDriverDepthStencilState* state)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!state || !ctx) return;
}

static void _setTextureState(RgDriverTextureState* states, unsigned stateCount, unsigned startingTextureUnit)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !states || stateCount == 0) return;
}

//////////////////////////////////////////////////////////////////////////
// Buffer

struct RgDriverBufferImpl : public RgDriverBuffer
{
	void* systemBuf;
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

	rhinoca_free(impl->systemBuf);
	delete static_cast<RgDriverBufferImpl*>(self);
}

//////////////////////////////////////////////////////////////////////////
// Texture

//////////////////////////////////////////////////////////////////////////
// Shader

struct RgDriverShaderImpl : public RgDriverShader
{
	ID3D11DeviceChild* dxShader;
};	// RgDriverShaderImpl

static RgDriverShader* _newShader()
{
	RgDriverShaderImpl* ret = new RgDriverShaderImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteShader(RgDriverShader* self)
{
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
	if(!impl) return;

	safeRelease(impl->dxShader);

	delete static_cast<RgDriverShaderImpl*>(self);
}

static bool _initShader(RgDriverShader* self, RgDriverShaderType type, const char** sources, unsigned sourceCount)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
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
	if(type == RgDriverShaderType_Vertex)
		hr = ctx->dxDevice->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)&impl->dxShader);
	else if(type == RgDriverShaderType_Geometry)
		hr = ctx->dxDevice->CreateGeometryShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)&impl->dxShader);
	else if(type == RgDriverShaderType_Pixel)
		hr = ctx->dxDevice->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)&impl->dxShader);
	else
		RHASSERT(false);

	safeRelease(shaderBlob);

	if(FAILED(hr)) {
		rhLog("error", "Fail to create shader\n");
		return false;
	}

	return true;
}

bool _bindShaders(RgDriverShader** shaders, unsigned shaderCount)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !shaders || shaderCount == 0) return false;

	for(unsigned i=0; i<shaderCount; ++i) {
		RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(shaders[i]);
		if(!shader) continue;

		if(shader->type == RgDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(static_cast<ID3D11VertexShader*>(shader->dxShader), NULL, 0);
		else if(shader->type == RgDriverShaderType_Pixel)
			ctx->dxDeviceContext->GSSetShader(static_cast<ID3D11GeometryShader*>(shader->dxShader), NULL, 0);
		else if(shader->type == RgDriverShaderType_Geometry)
			ctx->dxDeviceContext->PSSetShader(static_cast<ID3D11PixelShader*>(shader->dxShader), NULL, 0);
		else
			RHASSERT(false);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Making draw call

static void _drawTriangle(unsigned offset, unsigned vertexCount, unsigned flags)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
}

static void _drawTriangleIndexed(unsigned offset, unsigned indexCount, unsigned flags)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx) return;
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

RgDriver* _rhNewRenderDriver_DX11(const char* options)
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
//	ret->changeResolution = _driverChangeResolution_DX11;
	ret->setViewport = _setViewport;
	ret->clearColor = _clearColor;
	ret->clearDepth = _clearDepth;
	ret->clearStencil = _clearStencil;

	ret->setBlendState = _setBlendState;
	ret->setDepthStencilState = _setDepthStencilState;
	ret->setTextureState = _setTextureState;

	ret->newBuffer = _newBuffer;
	ret->deleteBuffer = _deleteBuffer;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;

	ret->bindShaders = _bindShaders;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}