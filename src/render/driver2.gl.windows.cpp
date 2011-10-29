#include "pch.h"
#include "driver2.h"

#include "../array.h"
#include "../rhassert.h"
#include "../rhlog.h"
#include "../vector.h"

#include <gl/gl.h>
#include "gl/glext.h"

#pragma comment(lib, "OpenGL32")
#pragma comment(lib, "GLU32")

#define _(DECLAR, NAME) DECLAR NAME = NULL;
#	include "win32/extensionswin32list.h"
#undef _

namespace {

#include "driver2.gl.inl"

struct ContextImpl : public RgDriverContextImpl
{
	HWND hWnd;
	HDC hDc;
	HGLRC hRc;
	Vector<RgDriverShaderInput> programInputCache;
};	// ContextImpl

}	// namespace

static bool _oglFunctionInited = false;

RgDriverContext* _newDriverContext_GL(RgDriver* driver)
{
	ContextImpl* ret = new ContextImpl;

	ret->driver = driver;
	ret->width = ret->height = 0;
	ret->magjorVersion = 2;
	ret->minorVersion = 2;

	ret->currentBlendStateHash = 0;
	ret->currentDepthStencilStateHash = 0;

	ret->currentIndexBufSysMemPtr = NULL;

	ret->currentShaderProgram = NULL;

	ret->currentPixelBufferIndex = 0;
	ret->pixelBufferCache.resize(ret->pixelBufferCache.capacity());

	RgDriverContextImpl::TextureState texState = { 0, 0 };
	ret->textureStateCache.assign(texState);

	ret->hWnd = NULL;
	ret->hDc = NULL;
	ret->hRc = NULL;

	return ret;
}

static ContextImpl* _currentContext = NULL;

void _deleteDriverContext_GL(RgDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

	// Destroy any shader program
	glUseProgram(0);
	for(unsigned i=0; i<impl->shaderProgramCache.size(); ++i) {
		glDeleteProgram(impl->shaderProgramCache[i].glh);
	}

	// Free the sampler state cache
	for(unsigned i=0; i<impl->textureStateCache.size(); ++i) {
		if(impl->textureStateCache[i].glh != 0)
			glDeleteSamplers(1, &impl->textureStateCache[i].glh);
	}

	// Free the pixel buffer cache
	glDeleteBuffers(impl->pixelBufferCache.size(), &impl->pixelBufferCache[0]);

	if(impl == _currentContext) {
		 wglMakeCurrent(NULL, NULL); 
		_currentContext = NULL;
	}

	wglDeleteContext(impl->hRc);

	delete static_cast<ContextImpl*>(self);
}

void _useDriverContext_GL(RgDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
}

RgDriverContext* _getCurrentContext_GL()
{
	return _currentContext;
}

static void initGlFunc()
{
	// Initialize opengl functions
	if(!_oglFunctionInited)
	{
		#define GET_FUNC_PTR(type, ptr) \
		if(!(ptr = (type) wglGetProcAddress(#ptr))) \
		{	ptr = (type) wglGetProcAddress(#ptr"EXT");	}

		#define _(DECLAR, NAME) GET_FUNC_PTR(DECLAR, NAME);
		#	include "win32/extensionswin32list.h"
		#undef _

		#undef GET_FUNC_PTR

		_oglFunctionInited = true;
	}
}

bool _initDriverContext_GL(RgDriverContext* self, void* platformSpecificWindow)
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

		if((hRc = wglCreateContextAttribsARB(hDc, 0, attribs)))
		{	// Delete the old GL context (GL2x and use the new one)
			ret = ::wglMakeCurrent(hDc, hRc) == TRUE;
			wglDeleteContext(impl->hRc);
			impl->hRc = hRc;
		}
	}

	impl->magjorVersion = v1;
	impl->minorVersion = v2;

	rhLog("verbose", "Using Opengl version: %u.%u\n", v1, v2);

	initGlFunc();

	impl->driver->applyDefaultState(impl);

	// Init pixel buffer cache
	glGenBuffers(impl->pixelBufferCache.size(), &impl->pixelBufferCache[0]);

	return ret;
}

void _driverSwapBuffers_GL()
{
	if(!_currentContext) {
		RHASSERT(false && "Please call RgDriver->useContext");
		return;
	}

	// Clean up not frequently used VAO
	if(glDeleteVertexArrays)
	for(unsigned i=0; i<_currentContext->vaoCache.size();) {
		float& hotness = _currentContext->vaoCache[i].hotness;
		hotness *= 0.5f;

		if(hotness < 0.0001f) {
			glDeleteVertexArrays(1, &_currentContext->vaoCache[i].glh);
			_currentContext->vaoCache.remove(i);
		}
		else
			++i;
	}

//	RHVERIFY(::SwapBuffers(_currentContext->hDc) == TRUE);
	::SwapBuffers(_currentContext->hDc);
}

bool _driverChangeResolution_GL(unsigned width, unsigned height)
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
