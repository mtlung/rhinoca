#define  _CRT_SECURE_NO_WARNINGS
#include "../src/rhinoca.h"

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>
#include <stdio.h>
#include <crtdbg.h>
#include <assert.h>

#pragma comment(lib, "OpenGL32")
#pragma comment(lib, "GLU32")

#ifdef NDEBUG
#	define ASSERT(Expression) ((void)0)
#	define VERIFY(Expression) ((void)(Expression))
#else
#	define ASSERT(Expression) assert(Expression)
#	define VERIFY(Expression) assert(Expression)
#endif

static const wchar_t* windowClass = L"Rhinoca Launcher";

static bool quitWindow = false;

LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Rhinoca* rh = reinterpret_cast<Rhinoca*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

	{
		RhinocaEvent ev = { uMsg, wParam, lParam };
		rhinoca_processevent(rh, ev);
	}

	switch(uMsg)
	{
	case WM_SIZE:
		rhinoca_setSize(rh, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_PAINT:
		::ValidateRect(hWnd, NULL);
		break;
	case WM_DESTROY:
		quitWindow = true;
		break;
	default:
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

HWND createWindow(HWND existingWindow, int width, int height, bool fullScreen)
{
	width = width <= 0 ? CW_USEDEFAULT : width;
	height = height <= 0 ? CW_USEDEFAULT : width;

	HWND hWnd = NULL;
	HMODULE hModule = ::GetModuleHandle(NULL);

	// Register window class
	WNDCLASS wc;
	::ZeroMemory(&wc, sizeof(wc));
	// CS_OWNDC is an optimization, see page 659 of OpenGL SuperBible 4th edition
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.hInstance = hModule;
	wc.lpfnWndProc = &wndProc;
	wc.lpszClassName = windowClass;
	wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;	// (HBRUSH)(COLOR_WINDOW);

	VERIFY(::RegisterClass(&wc) != 0);

	if(existingWindow == 0)
	{
		// OpenGL requires WS_CLIPCHILDREN and WS_CLIPSIBLINGS, see page 657 of OpenGL SuperBible 4th edition
		DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

		// In windowed mode, adjust width and height so that window will have the requested client area
		if(!fullScreen) {
			RECT rect = { 0, 0, width, height };
			::AdjustWindowRect(&rect, style, /*hasMenu*/false);
			width = (width == CW_USEDEFAULT ? width : rect.right - rect.left);
			height = (height == CW_USEDEFAULT ? height : rect.bottom - rect.top);
		}

		hWnd = ::CreateWindowW(
			windowClass, L"Rhinoca Launcher",
			style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			width, height,
			NULL, NULL,
			hModule,
			NULL
		);
	}
	else
	{
		hWnd = existingWindow;
	}

	{	// Get the client area size from Win32 if user didn't supply one
		if(width == CW_USEDEFAULT || height == CW_USEDEFAULT) {
			RECT rect;
			::GetClientRect(hWnd, &rect);
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}

//		if(fullScreen)
//			setFullscreen(true);
	}

	return hWnd;
}

bool initOpenGl(HWND hWnd, HDC& dc)
{
	dc = ::GetDC(hWnd);

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

	int pixelFormat = ::ChoosePixelFormat(dc, &pfd);
	if(::SetPixelFormat(dc, pixelFormat, &pfd) != TRUE) return false;
	HGLRC rc = ::wglCreateContext(dc);
	return ::wglMakeCurrent(dc, rc) == TRUE;
}

static void* ioOpen(Rhinoca* rh, const char* uri, int threadId)
{
	FILE* f = fopen(uri, "rb");
	return f;
}

static rhuint64 ioRead(void* file, void* buffer, rhuint64 size, int threadId)
{
	FILE* f = reinterpret_cast<FILE*>(file);
	return (rhuint64)fread(buffer, 1, (size_t)size, f);
}

static void ioClose(void* file, int threadId)
{
	FILE* f = reinterpret_cast<FILE*>(file);
	fclose(f);
}

void rhinoca_io_setcallback(rhinoca_io_open open, rhinoca_io_read read, rhinoca_io_close close);

int main()
{
#ifdef _MSC_VER
	// Tell the c-run time to do memory check at program shut down
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetBreakAlloc(-1);
#endif

	HWND hWnd = createWindow(NULL, -1, -1, false);

	HDC dc;
	initOpenGl(hWnd, dc);

	rhinoca_init();
	Rhinoca* rh = rhinoca_create(NULL);
	rhinoca_io_setcallback(ioOpen, ioRead, ioClose);

	// Associate Rhinoca context as the user-data of the window handle
	::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(rh));
	::ShowWindow(hWnd, true);

	rhinoca_openDocument(rh, "html5/test1/test.html");

	while(true) {
		MSG message;

		while(::PeekMessage(&message, hWnd, 0, 0, PM_REMOVE)) {
			(void)TranslateMessage(&message);
			(void)DispatchMessage(&message);

			if(quitWindow) break;
		}

		if(!quitWindow) {
			rhinoca_update(rh);
			VERIFY(::SwapBuffers(dc) == TRUE);
		} else
			break;
	}

	rhinoca_closeDocument(rh);

	rhinoca_destroy(rh);
	rhinoca_close();

	return 0;
}
