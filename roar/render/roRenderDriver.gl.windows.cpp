#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roStopWatch.h"
#include "../base/roTypeCast.h"
#include <stdio.h>

#include "../platform/roPlatformHeaders.h"
#include <gl/gl.h>
#include "../../src/render/gl/glext.h"

#pragma comment(lib, "OpenGL32")
#pragma comment(lib, "GLU32")

#define _(DECLAR, NAME) DECLAR NAME = NULL;
#	include "platform.win/extensionsList.h"
#undef _

using namespace ro;

static DefaultAllocator _allocator;

namespace {

#include "roRenderDriver.gl.inl"

struct ContextImpl : public roRDriverContextImpl
{
	HWND hWnd;
	HDC hDc;
	HGLRC hRc;
	StopWatch stopWatch;
	Array<roRDriverShaderBufferInput> programInputCache;
};

}	// namespace

static bool _oglFunctionInited = false;

roRDriverContext* _newDriverContext_GL(roRDriver* driver)
{
	ContextImpl* ret = _allocator.newObj<ContextImpl>().unref();

	ret->driver = driver;
	ret->width = ret->height = 0;
	ret->magjorVersion = 2;
	ret->minorVersion = 2;
	ret->frameCount = 0;
	ret->lastFrameDuration = 0;
	ret->lastSwapTime = 0;

	ret->currentBlendStateHash = 0;
	ret->currentRasterizerStateHash = 0;
	ret->currentDepthStencilStateHash = 0;

	ret->currentIndexBufSysMemPtr = NULL;

	ret->currentShaderProgram = NULL;

	ret->currentPixelBufferIndex = 0;
	ret->pixelBufferCache.resize(8);	// FIXME: Just an arbitrary number

	roRDriverContextImpl::TextureState texState = { 0, 0 };
	ret->textureStateCache.assign(texState);

	ret->currentDepthFunc = roRDriverCompareFunc(-1);
	ret->currentColorWriteMask = roRDriverColorWriteMask(-1);

	ret->hWnd = NULL;
	ret->hDc = NULL;
	ret->hRc = NULL;

	return ret;
}

static ContextImpl* _currentContext = NULL;

void _deleteDriverContext_GL(roRDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

	// Destroy any shader program
	glUseProgram(0);
	for(roSize i=0; i<impl->shaderProgramCache.size(); ++i) {
		glDeleteProgram(impl->shaderProgramCache[i].glh);
	}

	// Free the sampler state cache
	for(roSize i=0; i<impl->textureStateCache.size(); ++i) {
		if(impl->textureStateCache[i].glh != 0)
			glDeleteSamplers(1, &impl->textureStateCache[i].glh);
	}

	// Free the pixel buffer cache
	glDeleteBuffers(num_cast<GLsizei>(impl->pixelBufferCache.size()), &impl->pixelBufferCache[0]);

	// Free buffer object cache
	while(!impl->bufferCache.isEmpty()) {
		roRDriverBufferImpl* b = impl->bufferCache.back();
		impl->bufferCache.popBack();
		if(b->glh)
			glDeleteBuffers(1, &b->glh);

		roAssert(!b->isMapped);
		_allocator.free(b->systemBuf);
		_allocator.deleteObj(b);
	}

	if(impl == _currentContext) {
		 wglMakeCurrent(NULL, NULL); 
		_currentContext = NULL;
		roRDriverCurrentContext = NULL;
	}

	wglDeleteContext(impl->hRc);

	_allocator.deleteObj(static_cast<ContextImpl*>(self));
}

void _useDriverContext_GL(roRDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
	roRDriverCurrentContext = impl;
}

roRDriverContext* _getCurrentContext_GL()
{
	return _currentContext;
}

static void initGlFunc()
{
	// Initialize Opengl functions
	if(!_oglFunctionInited)
	{
		#define GET_FUNC_PTR(type, ptr) \
		if((ptr = (type) wglGetProcAddress(#ptr)) == NULL) \
		{	ptr = (type) wglGetProcAddress(#ptr"EXT");	}

		#define _(DECLAR, NAME) GET_FUNC_PTR(DECLAR, NAME);
		#	include "platform.win/extensionsList.h"
		#undef _

		#undef GET_FUNC_PTR

		_oglFunctionInited = true;
	}
}

bool _initDriverContext_GL(roRDriverContext* self, void* platformSpecificWindow)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return false;

	HWND hWnd = impl->hWnd = reinterpret_cast<HWND>(platformSpecificWindow);
	HDC hDc = impl->hDc = ::GetDC(hWnd);

	WINDOWINFO info;
	::GetWindowInfo(hWnd, &info);
	impl->width = info.rcClient.right - info.rcClient.left;
	impl->height = info.rcClient.bottom - info.rcClient.top;

	PIXELFORMATDESCRIPTOR pfd;
	::ZeroMemory(&pfd, sizeof(pfd));
	pfd.nVersion = 1;
	pfd.nSize = sizeof(pfd);
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormat = ::ChoosePixelFormat(hDc, &pfd);
	if(::SetPixelFormat(hDc, pixelFormat, &pfd) != TRUE)
		return false;

	HGLRC hRc = impl->hRc = ::wglCreateContext(hDc);
	bool ret = ::wglMakeCurrent(hDc, hRc) == TRUE;

	// Get the current Opengl version
	const char* version = (const char*)glGetString(GL_VERSION);
	unsigned v1, v2;
	sscanf(version, "%u.%u", &v1, &v2);

	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)(wglGetProcAddress("wglCreateContextAttribsARB"));

	// Create a newer driver if necessary
	if(wglCreateContextAttribsARB && v1 <= impl->magjorVersion && v2 < impl->minorVersion) {
		const int attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, impl->magjorVersion,
			WGL_CONTEXT_MINOR_VERSION_ARB, impl->minorVersion,
			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			0
		};

		if((hRc = wglCreateContextAttribsARB(hDc, 0, attribs)) == NULL)
		{	// Delete the old GL context (GL2x and use the new one)
			ret = ::wglMakeCurrent(hDc, hRc) == TRUE;
			wglDeleteContext(impl->hRc);
			impl->hRc = hRc;
		}
	}

	impl->magjorVersion = v1;
	impl->minorVersion = v2;

	roLog("verbose", "Using Opengl version: %u.%u\n", v1, v2);

	initGlFunc();

	// Disable v-sync
	wglSwapIntervalEXT(0);

	impl->driver->applyDefaultState(impl);

	// Init pixel buffer cache
	glGenBuffers(num_cast<GLsizei>(impl->pixelBufferCache.size()), &impl->pixelBufferCache[0]);

	return ret;
}

void _driverSwapBuffers_GL()
{
	if(!_currentContext) {
		roAssert(false && "Please call roRDriver->useContext");
		return;
	}

	static const float removalTimeOut = 5;

	// Clean up not frequently used input VAO
	if(glDeleteVertexArrays)
	for(roSize i=0; i<_currentContext->vaoCache.size();) {
		float lastUsedTime = _currentContext->vaoCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut) {
			glDeleteVertexArrays(1, &_currentContext->vaoCache[i].glh);
			_currentContext->vaoCache.remove(i);
		}
		else
			++i;
	}

	// Clean up not frequently used render target cache
	for(roSize i=0; i<_currentContext->renderTargetCache.size();) {
		float lastUsedTime = _currentContext->renderTargetCache[i].lastUsedTime;

		if(lastUsedTime < _currentContext->lastSwapTime - removalTimeOut) {
			glDeleteFramebuffers(1, &_currentContext->renderTargetCache[i].glh);
			_currentContext->renderTargetCache.remove(i);
		}
		else
			++i;
	}

	// TODO: Release buffer object's memory once a while

	// Clean up not frequently used depth stencil cache
	for(roSize i=0; i<_currentContext->depthStencilBufferCache.size();) {
		DepthStencilBuffer& cache = _currentContext->depthStencilBufferCache[i];

		if(cache.refCount == 0 && cache.lastUsedTime < _currentContext->lastSwapTime - removalTimeOut) {
			glDeleteRenderbuffers(3, &cache.depthHandle);
			_currentContext->depthStencilBufferCache.remove(i);
		}
		else
			++i;
	}

	// Perform the swap
	::SwapBuffers(_currentContext->hDc);

	// Update statistics
	++_currentContext->frameCount;
	float lastSwapTime = _currentContext->lastSwapTime;
	_currentContext->lastSwapTime = _currentContext->stopWatch.getFloat();
	_currentContext->lastFrameDuration = _currentContext->lastSwapTime - lastSwapTime;
}

bool _driverChangeResolution_GL(unsigned width, unsigned height)
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

	return true;
}
