#include "pch.h"
#include "driver2.h"

#include <gl/gl.h>
#include "gl/glext.h"

#include "../assert.h"
#include "../vector.h"

#pragma comment(lib, "OpenGL32")
#pragma comment(lib, "GLU32")

#define _(DECLAR, NAME) DECLAR NAME = NULL;
#	include "win32/extensionswin32list.h"
#undef _

static bool _oglFunctionInited = false;

struct ContextImpl : public RhRenderDriverContext
{
	HWND hWnd;
	HDC hDc;
	HGLRC hRc;
	Vector<RhRenderShaderProgramInput> programInputCache;
};	// ContextImpl

RhRenderDriverContext* _newDriverContext()
{
	ContextImpl* ret = new ContextImpl;
	ret->hWnd = NULL;
	ret->hDc = NULL;
	ret->width = ret->height = 0;
	return ret;
}

static ContextImpl* _currentContext = NULL;

void _useDriverContext(RhRenderDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
}

void _deleteDriverContext(RhRenderDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

	if(impl == _currentContext) {
		 wglMakeCurrent(NULL, NULL); 
		_currentContext = NULL;
	}

	wglDeleteContext(impl->hRc);

	delete static_cast<ContextImpl*>(self);
}

bool _initContext(RhRenderDriverContext* self, void* platformSpecificWindow)
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

	return ret;
}

void _swapBuffers(RhRenderDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return;

//	RHVERIFY(::SwapBuffers(impl->hDc) == TRUE);
	::SwapBuffers(impl->hDc);
}

bool _changeResolution(RhRenderDriverContext* self, unsigned width, unsigned height)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	if(!impl) return false;

	HWND hWnd = (HWND)impl->hWnd;
	RECT rcClient, rcWindow;
	POINT ptDiff;

	::GetClientRect(hWnd, &rcClient);
	::GetWindowRect(hWnd, &rcWindow);
	ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
	ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
	::SetWindowPos(hWnd, 0, rcWindow.left, rcWindow.top, width + ptDiff.x, height + ptDiff.y, SWP_NOMOVE|SWP_NOZORDER);

	impl->width = width;
	impl->height = height;

	return true;
}
