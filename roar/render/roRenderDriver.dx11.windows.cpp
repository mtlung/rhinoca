#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roCpuProfiler.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roStopWatch.h"
#include "../base/roStringHash.h"

#include <dxgi.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib" )

using namespace ro;

static DefaultAllocator _allocator;

//namespace {

#include "roRenderDriver.dx11.inl"

struct ContextImpl : public roRDriverContextImpl
{
	HWND hWnd;
	StopWatch stopWatch;
};

//}	// namespace

roRDriverContext* _newDriverContext_DX11(roRDriver* driver)
{
	ContextImpl* ret = _allocator.newObj<ContextImpl>().unref();
	if(!ret) return NULL;

	ret->driver = driver;
	ret->width = ret->height = 0;
	ret->majorVersion = 0;
	ret->minorVersion = 0;
	ret->frameCount = 0;
	ret->lastFrameDuration = 0;
	ret->lastSwapTime = 0;

	ret->currentBlendStateHash = 0;
	ret->currentRasterizerStateHash = 0;
	ret->currentDepthStencilStateHash = 0;

	ret->currentShaders.assign(NULL);

	ret->currentRenderTargetViewHash = 0;

	SamplerState texState = { 0, ComPtr<ID3D11SamplerState>(NULL) };
	ret->samplerStateCache.assign(texState);

	ret->hWnd = NULL;

	ret->triangleFanIndexBufferSize = 0;
	ret->triangleFanIndexBuffer = driver->newBuffer();

	ret->constBufferInUse.assign(NULL);

	return ret;
}

static ContextImpl* _currentContext = NULL;

void _deleteDriverContext_DX11(roRDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

	for(roSize i=0; i<impl->currentShaders.size(); ++i)
		roAssert(!impl->currentShaders[i] && "Please destroy all shaders before detroy the context");

	impl->driver->deleteBuffer(impl->triangleFanIndexBuffer);

	for(roRDriverBufferImpl*& i : impl->constBufferInUse) {
		// The same buffer may be used in multiple shader, only do the deletion for the last one
		if(roOccurrence(impl->constBufferInUse.begin(), impl->constBufferInUse.end(), i) == 1)
			impl->driver->deleteBuffer(i);
		i = NULL;
	}

	for(roSize i=0; i<impl->stagingBufferCache.size(); ++i) {
		roAssert(!impl->stagingBufferCache[i].mapped);
	}

	for(roSize i=0; i<impl->stagingTextureCache.size(); ++i) {
		roAssert(!impl->stagingTextureCache[i].mapped);
	}

	if(impl == _currentContext) {
		_currentContext = NULL;
		roRDriverCurrentContext = NULL;
	}

	// Change back to windowed mode before releasing swap chain
	if(impl->dxSwapChain)
		impl->dxSwapChain->SetFullscreenState(false, NULL);

	_allocator.deleteObj(impl);
}

void _useDriverContext_DX11(roRDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
	roRDriverCurrentContext = impl;
}

roRDriverContext* _getCurrentContext_DX11()
{
	return _currentContext;
}

static bool _initRenderTarget(ContextImpl* impl, const DXGI_SWAP_CHAIN_DESC& swapChainDesc)
{
	ID3D11Device* device = impl->dxDevice;
	ID3D11DeviceContext* immediateContext = impl->dxDeviceContext;
	IDXGISwapChain* swapChain = impl->dxSwapChain;

	// Create render target
	ComPtr<ID3D11Resource> backBuffer;
	swapChain->GetBuffer(0, __uuidof(backBuffer), reinterpret_cast<void**>(&backBuffer.ptr));

	ID3D11RenderTargetView* renderTargetView = NULL;
	HRESULT hr = device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);

	if(FAILED(hr)) {
		roLog("error", "CreateRenderTargetView failed\n");
		return false;
	}

	// Create depth - stencil buffer
	// reference: http://www.dx11.org.uk/3dcube.htm
	D3D11_TEXTURE2D_DESC depthDesc;
	roMemZeroStruct(depthDesc);
	depthDesc.Width = swapChainDesc.BufferDesc.Width;
	depthDesc.Height = swapChainDesc.BufferDesc.Height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	// may be DXGI_FORMAT_D32_FLOAT
	depthDesc.SampleDesc.Count = swapChainDesc.SampleDesc.Count;
	depthDesc.SampleDesc.Quality = swapChainDesc.SampleDesc.Quality;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;

	ID3D11Texture2D* depthStencilTexture = NULL;
	hr = device->CreateTexture2D(&depthDesc, NULL, &depthStencilTexture);

	if(FAILED(hr)) {
		roLog("error", "CreateTexture2D for depth-stencil texture failed\n");
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
	roMemZeroStruct(depthViewDesc);
	depthViewDesc.Format = depthDesc.Format;
	depthViewDesc.ViewDimension = swapChainDesc.SampleDesc.Count == 1 ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DMS;
	depthViewDesc.Texture2D.MipSlice = 0;

	ID3D11DepthStencilView* depthStencilView = NULL;
	hr = device->CreateDepthStencilView(depthStencilTexture, &depthViewDesc, &depthStencilView);

	if(FAILED(hr)) {
		roLog("error", "CreateDepthStencilView failed\n");
		return false;
	}

	immediateContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

	impl->dxRenderTargetView = renderTargetView;
	impl->dxDepthStencilTexture = depthStencilTexture;
	impl->dxDepthStencilView = depthStencilView;

	return true;
}

bool _initDriverContext_DX11(roRDriverContext* self, void* platformSpecificWindow)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return false;

	HWND hWnd = impl->hWnd = reinterpret_cast<HWND>(platformSpecificWindow);

	WINDOWINFO info;
	roMemZeroStruct(info);
	::GetWindowInfo(hWnd, &info);
	impl->width = info.rcClient.right - info.rcClient.left;
	impl->height = info.rcClient.bottom - info.rcClient.top;

	HRESULT hr;

	// Reference on device, context and swap chain:
	// http://msdn.microsoft.com/en-us/library/bb205075(VS.85).aspx

	// Create device and swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	roMemZeroStruct(swapChainDesc);
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = impl->width;
	swapChainDesc.BufferDesc.Height = impl->height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = 0;

	ID3D11Device* device = NULL;
	IDXGISwapChain* swapChain = NULL;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11DeviceContext* immediateContext = NULL;

	hr = D3D11CreateDeviceAndSwapChain(
		NULL,		// Which graphics adaptor to use, default is the first one returned by IDXGIFactory1::EnumAdapters
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,		// Software rasterizer
#if roDEBUG
//		D3D11_CREATE_DEVICE_DEBUG |
		D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS |
#endif
		D3D11_CREATE_DEVICE_SINGLETHREADED,
		NULL, 0,	// Feature level
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&swapChain,
		&device,
		&featureLevel,
		&immediateContext
	);

	if(FAILED(hr)) {
		roLog("error", "D3D11CreateDeviceAndSwapChain failed\n");
		return false;
	}

	swapChain->SetFullscreenState((BOOL)false, NULL);

	impl->dxSwapChain = swapChain;
	impl->dxDevice = device;
	impl->dxDeviceContext = immediateContext;

	impl->stagingBufferCacheSearchIndex = 0;
	impl->stagingTextureCacheSearchIndex = 0;

	// Create render target
	if(!_initRenderTarget(impl, swapChainDesc))
		return false;

	impl->driver->applyDefaultState(impl);

	return true;
}

void _driverSwapBuffers_DX11()
{
	if(!_currentContext) {
		roAssert(false && "Please call roRDriver->useContext");
		return;
	}

	roScopeProfile("roRenderDriver::swapBuffer");

	static const float removalTimeOut = 5;

	// Clean up not frequently used input layout cache
	for(roSize i=0; i<_currentContext->inputLayoutCache.size();) {
		float lastUsedTime = _currentContext->inputLayoutCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->inputLayoutCache.removeBySwapAt(i);
		else
			++i;
	}

	// Update and clean up on staging buffer cache
	// NOTE: We just set dxBuffer to null rather than remove the entry in 
	// stagingBufferCache, to avoid array container re-allocation invaliding pointer
	for(roSize i=0; i<_currentContext->stagingBufferCache.size();) {
		StagingBuffer& staging = _currentContext->stagingBufferCache[i];

		if(staging.dxBuffer && !staging.mapped && staging.lastUsedTime < _currentContext->lastSwapTime - removalTimeOut) {
			staging.size = 0;
			staging.dxBuffer = (ID3D11Resource*)NULL;
		}
		else
			++i;
	}

	// Clean up on staging texture cache
	for(roSize i=0; i<_currentContext->stagingTextureCache.size();) {
		StagingTexture& staging = _currentContext->stagingTextureCache[i];

		if(staging.dxTexture && !staging.mapped && staging.lastUsedTime < _currentContext->lastSwapTime - removalTimeOut) {
			staging.hash = 0;
			staging.dxTexture = (ID3D11Resource*)NULL;
		}
		else
			++i;
	}

	// Clean up on render target cache
	for(roSize i=0; i<_currentContext->renderTargetCache.size();) {
		RenderTarget& rt = _currentContext->renderTargetCache[i];

		if(rt.lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->renderTargetCache.removeBySwapAt(i);
		else
			++i;
	}

	// Clean up not frequently used blend cache
	for(roSize i=0; i<_currentContext->blendStateCache.size();) {
		float lastUsedTime = _currentContext->blendStateCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->blendStateCache.removeBySwapAt(i);
		else
			++i;
	}

	// Clean up not frequently used rasterizer cache
	for(roSize i=0; i<_currentContext->rasterizerState.size();) {
		float lastUsedTime = _currentContext->rasterizerState[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->rasterizerState.removeBySwapAt(i);
		else
			++i;
	}

	// Clean up not frequently used depth stencil cache
	for(roSize i=0; i<_currentContext->depthStencilStateCache.size();) {
		float lastUsedTime = _currentContext->depthStencilStateCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->depthStencilStateCache.removeBySwapAt(i);
		else
			++i;
	}

	// Clean up not frequently used buffer cache
	for(roSize i=0; i<_currentContext->bufferCache.size();) {
		float lastUsedTime = _currentContext->bufferCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut)
			_currentContext->bufferCache.removeBySwapAt(i);
		else
			++i;
	}

	int sync = 0;	// use 0 for no vertical sync
	roVerify(SUCCEEDED(_currentContext->dxSwapChain->Present(sync, 0)));

	// Update statistics
	++_currentContext->frameCount;
	float lastSwapTime = _currentContext->lastSwapTime;
	_currentContext->lastSwapTime = _currentContext->stopWatch.getFloat();
	_currentContext->lastFrameDuration = _currentContext->lastSwapTime - lastSwapTime;
}

bool _driverChangeResolution_DX11(unsigned width, unsigned height)
{
	if(!_currentContext) return false;

	if(width * height == 0) return true;	// Do nothing if the dimension is zero

	HWND hWnd = (HWND)_currentContext->hWnd;
	RECT rcClient, rcWindow;
	POINT ptDiff;

	::GetClientRect(hWnd, &rcClient);
	::GetWindowRect(hWnd, &rcWindow);
	ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
	ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
	::SetWindowPos(hWnd, 0, rcWindow.left, rcWindow.top, width + ptDiff.x, height + ptDiff.y, SWP_NOMOVE|SWP_NOZORDER);

	_currentContext->width = width;
	_currentContext->height = height;

	// Dealing with window resizing:
	// http://www.breaktrycatch.com/getting-started-with-directx-11/
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ee417025%28v=vs.85%29.aspx

	// Release all render target and view first
	_currentContext->dxDeviceContext->OMSetRenderTargets(0, NULL, NULL);
	_currentContext->dxRenderTargetView = (ID3D11RenderTargetView*)NULL;

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	_currentContext->dxSwapChain->GetDesc(&swapChainDesc);
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;

	HRESULT hr = _currentContext->dxSwapChain->ResizeBuffers(
		swapChainDesc.BufferCount, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

	if(!_initRenderTarget(_currentContext, swapChainDesc))
		return false;

	return SUCCEEDED(hr);
}
