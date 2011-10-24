#include "pch.h"
#include "driver2.h"

#include "../array.h"
#include "../rhassert.h"
#include "../rhlog.h"
#include "../vector.h"

#include <dxgi.h>
//#include <d3dcommon.h>
#include <d3d11.h>
//#include <d3dx10math.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "d3dx10.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib" )

namespace {

#include "driver2.dx11.inl"

template<typename T> void safeRelease(T*& t)
{
	if(t) t->Release();
	t = NULL;
}

struct ContextImpl : public RgDriverContextImpl
{
	HWND hWnd;

	Vector<RgDriverShaderInput> programInputCache;
};	// ContextImpl

}	// namespace

RgDriverContext* _newDriverContext_DX11(RgDriver* driver)
{
	ContextImpl* ret = new ContextImpl;

	ret->driver = driver;
	ret->width = ret->height = 0;
	ret->magjorVersion = 0;
	ret->minorVersion = 0;

	ret->currentBlendStateHash = 0;
	ret->currentDepthStencilStateHash = 0;

	ret->dxDevice = NULL;
	ret->dxDeviceContext = NULL;
	ret->dxSwapChain = NULL;
	ret->dxRenderTargetView = NULL;
	ret->dxDepthStencilTexture = NULL;
	ret->dxDepthStencilView = NULL;

	ret->currentShaders.assign(NULL);

	RgDriverContextImpl::TextureState texState = { 0 };
	ret->textureStateCache.assign(texState);

	ret->hWnd = NULL;

	return ret;
}

static ContextImpl* _currentContext = NULL;

void _deleteDriverContext_DX11(RgDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

	for(unsigned i=0; i<impl->currentShaders.size(); ++i)
		RHASSERT(!impl->currentShaders[i] && "Please destroy all shaders before detroy the context");

	// Free the sampler state cache
//	for(unsigned i=0; i<impl->textureStateCache.size(); ++i) {
//		if(impl->textureStateCache[i].glh != 0)
//			glDeleteSamplers(1, &impl->textureStateCache[i].glh);
//	}
		
	if(impl == _currentContext) {
		 wglMakeCurrent(NULL, NULL); 
		_currentContext = NULL;
	}

	// Change back to windowed mode before releasing swap chain
	impl->dxSwapChain->SetFullscreenState(false, NULL);

	safeRelease(impl->dxDepthStencilView);
	safeRelease(impl->dxDepthStencilTexture);
	safeRelease(impl->dxRenderTargetView);
	safeRelease(impl->dxSwapChain);
	safeRelease(impl->dxDeviceContext);
	safeRelease(impl->dxDevice);

	delete static_cast<ContextImpl*>(self);
}

void _useDriverContext_DX11(RgDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
}

RgDriverContext* _getCurrentContext_DX11()
{
	return _currentContext;
}

bool _initDriverContext_DX11(RgDriverContext* self, void* platformSpecificWindow)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return false;

	HWND hWnd = impl->hWnd = reinterpret_cast<HWND>(platformSpecificWindow);

	WINDOWINFO info;
	::GetWindowInfo(hWnd, &info);
	impl->width = info.rcClient.right - info.rcClient.left;
	impl->height = info.rcClient.bottom - info.rcClient.top;

	HRESULT hr;

	// Reference on device, context and swap chain:
	// http://msdn.microsoft.com/en-us/library/bb205075(VS.85).aspx

	// Create device and swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
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

	IDXGISwapChain* swapChain = NULL;
	ID3D11Device* device = NULL;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11DeviceContext* immediateContext = NULL;

	hr = D3D11CreateDeviceAndSwapChain(
		NULL,		// Which graphics adaptor to use, default is the first one returned by IDXGIFactory1::EnumAdapters
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,		// Software rasterizer
		D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
		NULL, 0,	// Feature level
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&swapChain,
		&device,
		&featureLevel,
		&immediateContext
	);

	if(FAILED(hr)) {
		rhLog("error", "D3D11CreateDeviceAndSwapChain failed\n");
		return false;
	}

	swapChain->SetFullscreenState((BOOL)false, NULL);

	// Create render target
	ID3D11Resource* backBuffer;
	swapChain->GetBuffer(0, __uuidof(backBuffer), reinterpret_cast<void**>(&backBuffer));

	ID3D11RenderTargetView* renderTargetView = NULL;
	hr = device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
	safeRelease(backBuffer);

	if(FAILED(hr)) {
		rhLog("error", "CreateRenderTargetView failed\n");
		return false;
	}

	// Create depth - stencil buffer
	// reference: http://www.dx11.org.uk/3dcube.htm
	D3D11_TEXTURE2D_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(depthDesc));
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
		rhLog("error", "CreateTexture2D for depth-stencil texture failed\n");
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
	ZeroMemory(&depthViewDesc, sizeof(depthViewDesc));
	depthViewDesc.Format = depthDesc.Format;
	depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthViewDesc.Texture2D.MipSlice = 0;

	ID3D11DepthStencilView* depthStencilView = NULL;
	hr = device->CreateDepthStencilView(depthStencilTexture, &depthViewDesc, &depthStencilView);

	if(FAILED(hr)) {
		rhLog("error", "CreateDepthStencilView failed\n");
		return false;
	}

	immediateContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

	// Create rasterizer state
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = true;	// NOTE: This is differ from DX11 initial value, which is false
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	ID3D11RasterizerState* rasterizerState;
	hr = device->CreateRasterizerState(&rasterDesc, &rasterizerState);
	immediateContext->RSSetState(rasterizerState);
	safeRelease(rasterizerState);

	if(FAILED(hr)) {
		rhLog("error", "CreateRasterizerState failed\n");
		return false;
	}

	impl->dxSwapChain = swapChain;
	impl->dxDevice = device;
	impl->dxDeviceContext = immediateContext;
	impl->dxRenderTargetView = renderTargetView;
	impl->dxDepthStencilTexture = depthStencilTexture;
	impl->dxDepthStencilView = depthStencilView;

	impl->driver->applyDefaultState(impl);

	return true;
}

void _driverSwapBuffers_DX11()
{
	if(!_currentContext) {
		RHASSERT(false && "Please call RgDriver->useContext");
		return;
	}

	int sync = 1;	// use 0 for not vertical sync
	RHVERIFY(SUCCEEDED(_currentContext->dxSwapChain->Present(sync, 0)));
}
/*
bool _driverChangeResolution_DX11(unsigned width, unsigned height)
{
	if(!_currentContext) return false;

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

	return true;
}
*/