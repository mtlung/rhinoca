#define  _CRT_SECURE_NO_WARNINGS
#include "../../src/rhinoca.h"

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <gl/gl.h>
//#define GL_GLEXT_PROTOTYPES
#include "../../src/render/gl/glext.h"
#include <stdio.h>
#include <crtdbg.h>
#include <assert.h>
#include <malloc.h>

#include "DropHandler.h"

#include "../../src/render/driver2.h"
#include "../../src/rhstring.h"

static const wchar_t* windowClass = L"Rhinoca Launcher";

static bool _quitWindow = false;

extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;

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
	glBindFramebuffer(GL_FRAMEBUFFER, 1);	// Handle '1' is the main buffer
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(userData);
	rhinoca_openDocument(rh, filePath);
}

static RhinocaRenderContext renderContext = { NULL, 0, 0, 0, 0 };

static bool setupFbo(unsigned width, unsigned height)
{
	if(!glGenFramebuffers)
		return false;

	// Generate texture
	if(!renderContext.texture) glGenTextures(1, &renderContext.texture);
	glBindTexture(GL_TEXTURE_2D, renderContext.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
	RHASSERT(GL_NO_ERROR == glGetError());

	// Generate frame buffer for depth and stencil
	if(!renderContext.depth) glGenRenderbuffers(1, &renderContext.depth);
	glBindRenderbuffer(GL_RENDERBUFFER, renderContext.depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	RHASSERT(GL_NO_ERROR == glGetError());

	// Create render target for Rhinoca to draw to
	if(!renderContext.fbo) glGenFramebuffers(1, &renderContext.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderContext.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderContext.texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderContext.depth);
	RHASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	RHASSERT(GL_NO_ERROR == glGetError());

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	return true;
}

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

	RHVERIFY(::RegisterClassW(&wc) != 0);

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

	RgDriver* driver = rhNewRenderDriver(NULL);
	RgDriverContext* context = driver->newContext();
	driver->initContext(context, hWnd);
	driver->useContext(context);

	rhinoca_init();

	if(!setupFbo(_width, _height)) {
		printf("Seems like your OpenGL driver didn't support 'Frame Buffer Object', program quit.");
		return 0;
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	Rhinoca* rh = rhinoca_create(&renderContext);
	rhinoca_setAlertFunc(alertFunc, hWnd);

	FileDropHandler dropHandler(hWnd, fileDropCallback, rh);

	// Associate Rhinoca context as the user-data of the window handle
	::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(rh));
	::ShowWindow(hWnd, true);

//	rhinoca_openDocument(rh, "../../test/htmlTest/audioTest/test.html");
//	rhinoca_openDocument(rh, "../../demo/impactjs/drop/drop.html");
	rhinoca_openDocument(rh, "../../demo/impactjs/biolab/biolab-ios.html");
//	rhinoca_openDocument(rh, "../../test/htmlTest/cssTest/bg.html");
//	rhinoca_openDocument(rh, "../../test.html");

	while(true) {
		MSG message;

		while(::PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
			(void)TranslateMessage(&message);
			(void)DispatchMessage(&message);

			if(_quitWindow) break;
		}

		if(!_quitWindow) {
			RHASSERT(GL_NO_ERROR == glGetError());

			glDepthMask(GL_FALSE);

			rhinoca_update(rh);
			RHASSERT(GL_NO_ERROR == glGetError());

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glViewport(0, 0, _width, _height);
			glClearColor(1, 1, 1, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, _width, _height, 0, 1, -1);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, renderContext.texture);

			glColor4f(1, 1, 1, 1);
			glBegin(GL_QUADS);
				glTexCoord2f(0, 0);
				glVertex3i(0, 0, 0);
				glTexCoord2f(1, 0);
				glVertex3i(_width, 0, 0);
				glTexCoord2f(1, 1);
				glVertex3i(_width, _height, 0);
				glTexCoord2f(0, 1);
				glVertex3i(0, _height, 0);
			glEnd();

			RHASSERT(GL_NO_ERROR == glGetError());
			driver->swapBuffers();
		} else
			break;
	}

	rhinoca_closeDocument(rh);

	rhinoca_destroy(rh);
	rhinoca_close();

	{	// Destroy the render context
		glDeleteRenderbuffers(1, &renderContext.stencil);
		glDeleteRenderbuffers(1, &renderContext.depth);
		glDeleteFramebuffers(1, &renderContext.fbo);
		glDeleteTextures(1, &renderContext.texture);
		wglDeleteContext(wglGetCurrentContext());
	}

	OleUninitialize();

	driver->deleteContext(context);
	rhDeleteRenderDriver(driver);

	return 0;
}
