#include "pch.h"
#include "../src/render/driver2.h"
#include "../src/rhassert.h"
#include "../src/rhstring.h"

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class GraphicsDriverTest
{
public:
	static const wchar_t* windowClass;

	GraphicsDriverTest() : hWnd(0), driver(NULL), context(NULL) {}
	~GraphicsDriverTest()
	{
		if(hWnd) {
			::ShowWindow(hWnd, false);
			::PostQuitMessage(0);
			driver->deleteContext(context);
			rhDeleteRenderDriver(driver);
			if(HMODULE hModule = ::GetModuleHandle(NULL))
				::UnregisterClassW(windowClass, hModule);
		}
	}

	static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if(uMsg == WM_CLOSE) PostQuitMessage(0);
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	void createWindow(int width, int height)
	{
		width = width <= 0 ? CW_USEDEFAULT : width;
		height = height <= 0 ? CW_USEDEFAULT : height;

		hWnd = NULL;
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

		// OpenGL requires WS_CLIPCHILDREN and WS_CLIPSIBLINGS, see page 657 of OpenGL SuperBible 4th edition
		DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

		// In windowed mode, adjust width and height so that window will have the requested client area
		RECT rect = { 0, 0, width, height };
		::AdjustWindowRect(&rect, style, /*hasMenu*/false);
		width = (width == CW_USEDEFAULT ? width : rect.right - rect.left);
		height = (height == CW_USEDEFAULT ? height : rect.bottom - rect.top);

		hWnd = ::CreateWindowW(
			windowClass, L"Rhinoca unit test",
			style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			width, height,
			NULL, NULL,
			hModule,
			NULL
		);

		::ShowWindow(hWnd, true);

		initContext();
	}

	void initContext()
	{
		driver = rhNewRenderDriver(NULL);
		context = driver->newContext();
		driver->initContext(context, hWnd);
		driver->useContext(context);
	}

	bool keepRun()
	{
		MSG msg = { 0 };
		if(::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			(void)TranslateMessage(&msg);
			(void)DispatchMessage(&msg);

			if(msg.message == WM_QUIT)
				return false;
		}

		return true;
	}

	HWND hWnd;
	RhRenderDriver* driver;
	RhRenderDriverContext* context;
};

const wchar_t* GraphicsDriverTest::windowClass = L"Rhinoca unit test";

TEST_FIXTURE(GraphicsDriverTest, empty)
{
	createWindow(1, 1);
}

TEST_FIXTURE(GraphicsDriverTest, basic)
{
	// To draw a full screen quad:
	// http://stackoverflow.com/questions/2588875/whats-the-best-way-to-draw-a-fullscreen-quad-in-opengl-3-2

	createWindow(200, 200);

	// Create shader
	RhRenderShader* vShader = driver->newShader();
	RhRenderShader* pShader = driver->newShader();
	RhRenderShaderProgram* program = driver->newShaderPprogram();

	const char* vShaderSrc =
		"attribute vec4 vertex;"
		"void main(void){gl_Position=vertex;}";
	driver->initShader(vShader, RhRenderShaderType_Vertex, &vShaderSrc, 1);

	const char* pShaderSrc =
		"uniform vec4 u_color;"
		"void main(void){gl_FragColor=u_color;}";
	driver->initShader(pShader, RhRenderShaderType_Pixel, &pShaderSrc, 1);

	RhRenderShader* shaders[] = { vShader, pShader };
	driver->initShaderProgram(program, shaders, 2);
	float c[] = { 1, 1, 0, 1 };
	driver->setUniform4fv(program, StringHash("u_color"), c, 1);

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	RhRenderBuffer* vbuffer = driver->newBuffer();
	driver->initBuffer(vbuffer, RhRenderBufferType_Vertex, vertex, sizeof(vertex));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RhRenderBuffer* ibuffer = driver->newBuffer();
	driver->initBuffer(ibuffer, RhRenderBufferType_Index, index, sizeof(index));

	// Bind shader input layout
	RhRenderShaderProgramInput input[] = {
		{ vbuffer, "vertex", 4, 0, 0, 0, 0 },
		{ ibuffer, NULL, 1, 0, 0, 0, 0 },
	};
	driver->bindProgramInput(program, input, COUNTOF(input), NULL);

	driver->setViewport(0, 0, context->width, context->height);

	while(keepRun()) {
		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteShaderProgram(program);
	driver->deleteShader(vShader);
	driver->deleteShader(pShader);
}
