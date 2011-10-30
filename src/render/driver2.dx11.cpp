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

template<typename T> void safeRelease(T*& t)
{
	if(t) t->Release();
	t = NULL;
}

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

	ID3D11BlendState* s = NULL;
	HRESULT hr = ctx->dxDevice->CreateBlendState(&desc, &s);

	if(FAILED(hr)) {
		rhLog("error", "CreateBlendState failed\n");
		return;
	}

	ctx->dxDeviceContext->OMSetBlendState(
		s,
		0,	// The blend factor, which to use with D3D11_BLEND_BLEND_FACTOR, but we didn't support it yet
		-1	// Bit mask to do with the multi-sample, not used so set all bits to 1
	);
	safeRelease(s);
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

	ID3D11DepthStencilState* s = NULL;
	HRESULT hr = ctx->dxDevice->CreateDepthStencilState(&desc, &s);

	if(FAILED(hr)) {
		rhLog("error", "CreateDepthStencilState failed\n");
		return;
	}

	ctx->dxDeviceContext->OMSetDepthStencilState(s, state->stencilRefValue);
	safeRelease(s);
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
	D3D11_BIND_FLAG(-1),
	D3D11_BIND_VERTEX_BUFFER,
	D3D11_BIND_INDEX_BUFFER,
	D3D11_BIND_CONSTANT_BUFFER
};

static const Array<D3D11_USAGE, 4> _bufferUsage = {
	D3D11_USAGE(-1),
	D3D11_USAGE_DEFAULT,
	D3D11_USAGE_STAGING,
	D3D11_USAGE_DYNAMIC
};

static bool _initBuffer(RgDriverBuffer* self, RgDriverBufferType type, RgDriverDataUsage usage, void* initData, unsigned sizeInBytes)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	self->type = type;
	self->sizeInBytes = sizeInBytes;

	D3D11_BIND_FLAG flag = _bufferBindFlag[type];

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = _bufferUsage[usage];
	desc.ByteWidth = sizeInBytes;
	desc.BindFlags = flag;
	desc.CPUAccessFlags = (usage == RgDriverDataUsage_Static) ? 0 : D3D11_CPU_ACCESS_WRITE;
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

static bool _updateBuffer(RgDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(offsetInBytes + sizeInBytes > self->sizeInBytes)
		return false;

 	if(impl->usage == RgDriverDataUsage_Stream)
 		memcpy(((char*)impl->systemBuf) + offsetInBytes, data, sizeInBytes);
 	else
	{
		// There are restriction on updating constant buffer
		if(impl->type == RgDriverBufferType_Uniform) {
			if(offsetInBytes != 0 && sizeInBytes != impl->sizeInBytes) {
				rhLog("error", "DX11 didn't support partial update of constant buffer\n");
				return false;
			}
		}

		ctx->dxDeviceContext->UpdateSubresource(impl->dxBuffer, 0, NULL, data, 0, 0);
	}

	return true;
}

static void* _mapBuffer(RgDriverBuffer* self, RgDriverBufferMapUsage usage)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	if(impl->systemBuf)
		return impl->systemBuf;

	if(!impl->dxBuffer)
		return NULL;

	if(impl->type == RgDriverBufferType_Uniform) {
		if(usage & RgDriverBufferMapUsage_Read) {
			rhLog("error", "DX11 driver didn't support read back of constant buffer\n");
			return NULL;
		}
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	HRESULT hr = ctx->dxDeviceContext->Map(
		impl->dxBuffer,
		0,
		D3D11_MAP_WRITE_DISCARD,	// TODO: Revise it later
		0,
		&mapped
	);

	if(FAILED(hr)) {
		rhLog("error", "Fail to map buffer\n");
		return NULL;
	}

	return mapped.pData;
}

static void _unmapBuffer(RgDriverBuffer* self)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!ctx || !impl) return;

	if(impl->systemBuf) {
		// Nothing need to do to un-map the system memory
		return;
	}

	if(!impl->dxBuffer)
		return;

	ctx->dxDeviceContext->Unmap(impl->dxBuffer, 0);
}

//////////////////////////////////////////////////////////////////////////
// Texture

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

	safeRelease(impl->dxShader);
	safeRelease(impl->dxShaderBlob);

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
	ID3D11ShaderReflection* reflector = NULL; 
	D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);

    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

	for(unsigned i=0; i<shaderDesc.BoundResources; ++i) {
		D3D11_SHADER_INPUT_BIND_DESC desc;
		hr = reflector->GetResourceBindingDesc(i, &desc);
		if(FAILED(hr))
			break;

		ConstantBuffer cb = { StringHash(desc.Name, 0), desc.BindPoint };
		impl->constantBuffers.push_back(cb);
	}

	safeRelease(reflector);

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

	if(shader->type == RgDriverShaderType_Vertex)
		ctx->dxDeviceContext->VSSetConstantBuffers(cb->bindPoint, 1, &bufferImpl->dxBuffer);
	else if(shader->type == RgDriverShaderType_Pixel)
		ctx->dxDeviceContext->PSSetConstantBuffers(cb->bindPoint, 1, &bufferImpl->dxBuffer);
	else if(shader->type == RgDriverShaderType_Geometry)
		ctx->dxDeviceContext->GSSetConstantBuffers(cb->bindPoint, 1, &bufferImpl->dxBuffer);

	return true;
}

bool _bindShaderInput(RgDriverShaderInput* inputs, unsigned inputCount, unsigned* cacheId)
{
	RgDriverContextImpl* ctx = static_cast<RgDriverContextImpl*>(_getCurrentContext_DX11());
	if(!ctx || !inputs || inputCount == 0) return false;

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
		// Bind vertex buffer
		else if(buffer->type == RgDriverBufferType_Vertex)
		{
			RgDriverShaderImpl* shader = static_cast<RgDriverShaderImpl*>(input->shader);
			if(!shader) continue;

			D3D11_INPUT_ELEMENT_DESC inputDesc;
			inputDesc.SemanticName = input->name;
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

			ID3D11InputLayout* layout = NULL;
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
			UINT stride = input->stride;
			UINT offset = input->offset;
			ctx->dxDeviceContext->IASetVertexBuffers(0, 1, &buffer->dxBuffer, &stride, &offset);
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

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;

	ret->bindShaders = _bindShaders;
	ret->bindShaderInput = _bindShaderInput;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}
