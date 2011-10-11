#include "pch.h"
#include "driver2.h"

#include <gl/gl.h>
#include "gl/glext.h"

#include "../rhassert.h"
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
	ret->magjorVersion = 2;
	ret->minorVersion = 0;
	return ret;
}

static ContextImpl* _currentContext = NULL;

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

void _useDriverContext(RhRenderDriverContext* self)
{
	ContextImpl* impl = static_cast<ContextImpl*>(self);
	_currentContext = impl;
}

bool _initDriverContext(RhRenderDriverContext* self, void* platformSpecificWindow)
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

	if(wglCreateContextAttribsARB) {
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

			// 3.0 or above requires a vertex array
			GLuint vertexArray;
			glGenVertexArrays(1, &vertexArray);
			glBindVertexArray(vertexArray);
		}
	}

	return ret;
}

void _driverSwapBuffers()
{
	if(!_currentContext) {
		RHASSERT(false && "Please call RhRenderDriver->useContext");
		return;
	}

//	RHVERIFY(::SwapBuffers(_currentContext->hDc) == TRUE);
	::SwapBuffers(_currentContext->hDc);
}

bool _driverChangeResolution(unsigned width, unsigned height)
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
