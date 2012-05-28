#define  _CRT_SECURE_NO_WARNINGS
#include "../../src/rhinoca.h"

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <crtdbg.h>
#include <assert.h>

#include "DropHandler.h"

#include "../../roar/base/roString.h"

static const wchar_t* windowClass = L"Rhinoca Launcher";

static bool _quitWindow = false;

struct RhinocaRenderContext
{
	void* platformSpecificContext;
	unsigned fbo;
	unsigned depth;
	unsigned stencil;
	unsigned texture;
};

int _width = 500;
int _height = 500;

void fileDropCallback(const char* filePath, void* userData)
{
	Rhinoca* rh = reinterpret_cast<Rhinoca*>(userData);
	rhinoca_openDocument(rh, filePath);
}

static RhinocaRenderContext renderContext = { NULL, 0, 0, 0, 0 };

LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Rhinoca* rh = reinterpret_cast<Rhinoca*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if(!rh)
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);

	{
		RhinocaEvent ev = { (void*)uMsg, wParam, lParam, 0, 0 };
		rhinoca_processEvent(rh, ev);
	}

	switch(uMsg)
	{
	case WM_SIZE:
		// TODO: Keep the data in the fbo texture during resize
		_width = LOWORD(lParam);
		_height = HIWORD(lParam);
		rhinoca_setSize(rh, _width, _height);
		break;
	case WM_PAINT:
		::ValidateRect(hWnd, NULL);
		break;
	case WM_DESTROY:
		_quitWindow = true;
		break;
	default:
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

HWND createWindow(HWND existingWindow, int& width, int& height, bool fullScreen)
{
	width = width <= 0 ? CW_USEDEFAULT : width;
	height = height <= 0 ? CW_USEDEFAULT : height;

	HWND hWnd = NULL;
	HMODULE hModule = ::GetModuleHandle(NULL);

	// Register window class
	WNDCLASSW wc;
	::ZeroMemory(&wc, sizeof(wc));
	// CS_OWNDC is an optimization, see page 659 of OpenGL SuperBible 4th edition
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.hInstance = hModule;
	wc.lpfnWndProc = &wndProc;
	wc.lpszClassName = windowClass;
	wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;	// (HBRUSH)(COLOR_WINDOW);

	roVerify(::RegisterClassW(&wc) != 0);

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

	{	// Get the client area size from Win32
		RECT rect;
		::GetClientRect(hWnd, &rect);
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;

//		if(fullScreen)
//			setFullscreen(true);
	}

	return hWnd;
}

void alertFunc(Rhinoca* rh, void* userData, const char* str)
{
	MessageBoxA((HWND)userData, str, "Javascript alert", MB_OK);
}

RHINOCA_API void rhinoca_setAlertFunc(rhinoca_alertFunc alertFunc);

int main()
{
#ifdef _MSC_VER
	// Tell the c-run time to do memory check at program shut down
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetBreakAlloc(-1);
#endif

	OleInitialize(NULL);

	HWND hWnd = createWindow(NULL, _width, _height, false);

	renderContext.platformSpecificContext = hWnd;

	rhinoca_init();

	Rhinoca* rh = rhinoca_create(&renderContext);
	rhinoca_setAlertFunc(alertFunc, hWnd);

	FileDropHandler dropHandler(hWnd, fileDropCallback, rh);

	// Associate Rhinoca context as the user-data of the window handle
	::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(rh));
	::ShowWindow(hWnd, true);

//	rhinoca_openDocument(rh, "../../test/htmlTest/audioTest/test.html");
//	rhinoca_openDocument(rh, "../../demo/impactjs/drop/drop.html");
//	rhinoca_openDocument(rh, "../../demo/impactjs/biolab/biolab-ios.html");
//	rhinoca_openDocument(rh, "../../test/htmlTest/cssTest/bg.html");
	rhinoca_openDocument(rh, "../../test/htmlTest/vgTest/LineTo.html");
//	rhinoca_openDocument(rh, "http://playbiolab.com/");

	while(true) {
		MSG message;

		while(::PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
			(void)TranslateMessage(&message);
			(void)DispatchMessage(&message);

			if(_quitWindow) break;
		}

		if(!_quitWindow)
			rhinoca_update(rh);
		else
			break;
	}

	rhinoca_closeDocument(rh);

	rhinoca_destroy(rh);
	rhinoca_close();

	OleUninitialize();

	return 0;
}
