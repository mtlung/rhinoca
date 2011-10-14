#include "pch.h"
#include "../src/render/driver2.h"
#include "../src/render/rgutility.h"
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
		if(driver) {
			driver->deleteShaderProgram(program);
			driver->deleteShader(vShader);
			driver->deleteShader(pShader);
		}

		if(hWnd) {
			::ShowWindow(hWnd, false);
			::PostQuitMessage(0);
			while(keepRun()) {}
			driver->deleteContext(context);
			rhDeleteRenderDriver(driver);
			::DestroyWindow(hWnd);
		}

		if(HMODULE hModule = ::GetModuleHandle(NULL))
			::UnregisterClassW(windowClass, hModule);
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
		context->magjorVersion = 3;
		context->minorVersion = 2;
		driver->initContext(context, hWnd);
		driver->useContext(context);

		driver->setViewport(0, 0, context->width, context->height);

		vShader = driver->newShader();
		pShader = driver->newShader();
		program = driver->newShaderPprogram();
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
	RgDriver* driver;
	RgDriverContext* context;
	RgDriverShader* vShader;
	RgDriverShader* pShader;
	RgDriverShaderProgram* program;
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

	// Init shader
	const char* vShaderSrc =
		"attribute vec4 vertex;"
		"void main(void){gl_Position=vertex;}";
	CHECK(driver->initShader(vShader, RgDriverShaderType_Vertex, &vShaderSrc, 1));

	const char* pShaderSrc =
		"uniform vec4 u_color;"
		"void main(void){gl_FragColor=u_color;}";
	CHECK(driver->initShader(pShader, RgDriverShaderType_Pixel, &pShaderSrc, 1));

	RgDriverShader* shaders[] = { vShader, pShader };
	CHECK(driver->initShaderProgram(program, shaders, 2));
	float c[] = { 1, 1, 0, 1 };
	CHECK(driver->setUniform4fv(program, StringHash("u_color"), c, 1));

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	RgDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, index, sizeof(index)));

	// Bind shader input layout
	RgDriverShaderProgramInput input[] = {
		{ vbuffer, "vertex", 4, 0, 0, 0, 0 },
		{ ibuffer, NULL, 1, 0, 0, 0, 0 },
	};
	CHECK(driver->bindProgramInput(program, input, COUNTOF(input), NULL));

	while(keepRun()) {
		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
}

TEST_FIXTURE(GraphicsDriverTest, 3d)
{
	createWindow(200, 200);

	// Init shader
	const char* vShaderSrc =
		"attribute vec3 vertex;"
		"uniform mat4 modelViewMat;"
		"uniform mat4 projectionMat;"
		"void main(void){gl_Position=(projectionMat*modelViewMat)*vec4(vertex,1);}";
	CHECK(driver->initShader(vShader, RgDriverShaderType_Vertex, &vShaderSrc, 1));

	const char* pShaderSrc =
		"uniform vec4 u_color;"
		"void main(void){gl_FragColor=u_color;}";
	CHECK(driver->initShader(pShader, RgDriverShaderType_Pixel, &pShaderSrc, 1));

	RgDriverShader* shaders[] = { vShader, pShader };
	driver->initShaderProgram(program, shaders, 2);
	float c[] = { 1, 1, 0, 1 };
	CHECK(driver->setUniform4fv(program, StringHash("u_color"), c, 1));

	// Create vertex buffer
	float vertex[][3] = { {-1,1,0}, {-1,-1,0}, {1,-1,0}, {1,1,0} };
	RgDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, index, sizeof(index)));

	// Bind shader input layout
	RgDriverShaderProgramInput input[] = {
		{ vbuffer, "vertex", 3, 0, 0, 0, 0 },
		{ ibuffer, NULL, 1, 0, 0, 0, 0 },
	};
	CHECK(driver->bindProgramInput(program, input, COUNTOF(input), NULL));

	// Model view matrix
	float modelView[16];
	rgMat44MakeIdentity(modelView);
	float translate[] =  { 0, 0, -3 };
	rgMat44TranslateBy(modelView, translate);
	CHECK(driver->setUniformMat44fv(program, StringHash("modelViewMat"), false, modelView, 1));

	// Projection matrix
	float prespective[16];
	rgMat44MakePrespective(prespective, 90, 1, 2, 10);
	CHECK(driver->setUniformMat44fv(program, StringHash("projectionMat"), false, prespective, 1));

	while(keepRun()) {
		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
}

TEST_FIXTURE(GraphicsDriverTest, blending)
{
	createWindow(200, 200);

	// Init shader
	const char* vShaderSrc =
		"attribute vec4 vertex;"
		"void main(void){gl_Position=vertex;}";
	CHECK(driver->initShader(vShader, RgDriverShaderType_Vertex, &vShaderSrc, 1));

	const char* pShaderSrc =
		"uniform vec4 u_color;"
		"void main(void){gl_FragColor=u_color;}";
	CHECK(driver->initShader(pShader, RgDriverShaderType_Pixel, &pShaderSrc, 1));

	RgDriverShader* shaders[] = { vShader, pShader };
	CHECK(driver->initShaderProgram(program, shaders, 2));

	// Create vertex buffer
	float vertex1[][4] = { {-1,1,0,1}, {-1,-0.5f,0,1}, {0.5f,-0.5f,0,1}, {0.5f,1,0,1} };
	RgDriverBuffer* vbuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer1, RgDriverBufferType_Vertex, vertex1, sizeof(vertex1)));

	float vertex2[][4] = { {-0.5f,0.5f,0,1}, {-0.5f,-1,0,1}, {1,-1,0,1}, {1,0.5f,0,1} };
	RgDriverBuffer* vbuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer2, RgDriverBufferType_Vertex, vertex2, sizeof(vertex2)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, index, sizeof(index)));

	// Set the blend state
	RgDriverBlendState blend = {
		0, 1,
		RgDriverBlendOp_Add, RgDriverBlendOp_Add,
		RgDriverBlendValue_SrcAlpha, RgDriverBlendValue_InvSrcAlpha,
		RgDriverBlendValue_One, RgDriverBlendValue_Zero
	};

	driver->setBlendState(&blend);

	while(keepRun())
	{
		{	// Draw the first quad
			RgDriverShaderProgramInput input[] = {
				{ vbuffer1, "vertex", 4, 0, 0, 0, 0 },
				{ ibuffer, NULL, 1, 0, 0, 0, 0 },
			};
			CHECK(driver->bindProgramInput(program, input, COUNTOF(input), NULL));

			float c[] = { 1, 1, 0, 0.5f };
			CHECK(driver->setUniform4fv(program, StringHash("u_color"), c, 1));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		{	// Draw the second quad
			RgDriverShaderProgramInput input[] = {
				{ vbuffer2, "vertex", 4, 0, 0, 0, 0 },
				{ ibuffer, NULL, 1, 0, 0, 0, 0 },
			};
			CHECK(driver->bindProgramInput(program, input, COUNTOF(input), NULL));

			float c[] = { 1, 0, 0, 0.5f };
			CHECK(driver->setUniform4fv(program, StringHash("u_color"), c, 1));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer1);
	driver->deleteBuffer(vbuffer2);
	driver->deleteBuffer(ibuffer);
}
