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
	ID3D11Buffer* dxBuffer;
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

	safeRelease(impl->dxBuffer);

	rhinoca_free(impl->systemBuf);
	delete static_cast<RgDriverBufferImpl*>(self);
}

static const Array<D3D11_BIND_FLAG, 4> _bufferBindFlag = {
	D3D11_BIND_FLAG(0),
	D3D11_BIND_VERTEX_BUFFER,
	D3D11_BIND_INDEX_BUFFER,
	D3D11_BIND_CONSTANT_BUFFER
};

static bool _initBuffer(RgDriverBuffer* self, RgDriverBufferType type, void* initData, unsigned sizeInBytes)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	self->type = type;
	self->sizeInBytes = sizeInBytes;

	D3D11_BIND_FLAG flag = _bufferBindFlag[type & (~RgDriverBufferType_System)];

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeInBytes;
	desc.BindFlags = flag;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = ctx->dxDevice->CreateBuffer(&desc, &data, &impl->dxBuffer);

	if(FAILED(hr)) {
		rhLog("error", "Fail to create buffer\n");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Texture

//////////////////////////////////////////////////////////////////////////
// Shader

static RgDriverShader* _newShader()
{
	RgDriverShaderImpl* ret = new RgDriverShaderImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteShader(RgDriverShader* self)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
	if(!ctx || !impl) return;

	for(unsigned i=0; i<ctx->currentShaders.size(); ++i)
		if(ctx->currentShaders[i] == impl) ctx->currentShaders[i] = NULL;

	safeRelease(impl->dxShader);
	safeRelease(impl->dxShaderBlob);

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

	return true;
}

bool _bindShaders(RgDriverShader** shaders, unsigned shaderCount)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !shaders || shaderCount == 0) return false;

	ctx->currentShaders.assign(NULL);

	// Bind used shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(shaders[i]);
		if(!shader) continue;

		ctx->currentShaders[shader->type] = shader;
		if(shader->type == RgDriverShaderType_Vertex)
			ctx->dxDeviceContext->VSSetShader(static_cast<ID3D11VertexShader*>(shader->dxShader), NULL, 0);
		else if(shader->type == RgDriverShaderType_Pixel)
			ctx->dxDeviceContext->PSSetShader(static_cast<ID3D11PixelShader*>(shader->dxShader), NULL, 0);
		else if(shader->type == RgDriverShaderType_Geometry)
			ctx->dxDeviceContext->GSSetShader(static_cast<ID3D11GeometryShader*>(shader->dxShader), NULL, 0);
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

bool _bindShaderInput(RgDriverShaderInput* inputs, unsigned inputCount, unsigned* cacheId)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs || inputCount == 0) return false;

	for(unsigned i=0; i<inputCount; ++i)
	{
		RgDriverBufferImpl* buffer = static_cast<RgDriverBufferImpl*>(inputs[i].buffer);
		if(!buffer) continue;

		if(buffer->type == RgDriverBufferType_Index) {
			ctx->dxDeviceContext->IASetIndexBuffer(buffer->dxBuffer, DXGI_FORMAT_R16_UINT, 0);
			continue;
		}

		RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(inputs[i].shader);
		if(!shader) continue;

		D3D11_INPUT_ELEMENT_DESC inputDesc;
		inputDesc.SemanticName = inputs[i].name;
		inputDesc.SemanticIndex = 0;
		inputDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// TODO: Fix me
		inputDesc.InputSlot = 0;
		inputDesc.AlignedByteOffset = 0;
		inputDesc.InputSlotClass =  D3D11_INPUT_PER_VERTEX_DATA;
		inputDesc.InstanceDataStepRate = 0;

		if(!ctx->currentShaders[buffer->type]) {
			rhLog("error", "Call bindShaders() before calling bindShaderInput()\n"); 
			return false;
		}

		ID3D11InputLayout* layout;
		ID3D10Blob* shaderBlob = shader->dxShaderBlob;

		if(!shaderBlob)
			return false;

		HRESULT hr = ctx->dxDevice->CreateInputLayout(
			&inputDesc, 1,
			shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
			&layout
		);

		ctx->dxDeviceContext->IASetInputLayout(layout);
		safeRelease(layout);

		if(FAILED(hr)) {
			rhLog("error", "Fail to CreateInputLayout\n");
			return false;
		}

		// TOOD: IASetVertexBuffers() should invoked once but not in a loop
		UINT stride = inputs[i].stride;
		UINT offset = inputs[i].offset;
		ctx->dxDeviceContext->IASetVertexBuffers(0, 1, &buffer->dxBuffer, &stride, &offset);
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
	ret->initBuffer = _initBuffer;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;

	ret->bindShaders = _bindShaders;
	ret->bindShaderInput = _bindShaderInput;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}
