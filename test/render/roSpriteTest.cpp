#include "pch.h"
#include "../../roar/render/roRenderDriver.h"
#include "../../roar/base/roStopWatch.h"
#include "../../roar/base/roStringHash.h"
#include "../../src/render/rgutility.h"

#include <math.h>

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace ro;

class SpriteTest
{
public:
	static const wchar_t* windowClass;

	SpriteTest() : hWnd(0), driver(NULL), context(NULL), averageFrameDuration(0) {}
	~SpriteTest()
	{
		if(driver) {
			driver->deleteShader(vShader);
			driver->deleteShader(gShader);
			driver->deleteShader(pShader);
		}

		if(hWnd) {
			::ShowWindow(hWnd, false);
			::PostQuitMessage(0);
			while(keepRun()) {}
			if(driver) driver->deleteContext(context);
			rgDeleteRenderDriver(driver);
			::DestroyWindow(hWnd);
		}

		if(HMODULE hModule = ::GetModuleHandle(NULL))
			::UnregisterClassW(windowClass, hModule);
	}

	static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		SpriteTest* test = reinterpret_cast<SpriteTest*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
		if(uMsg == WM_CLOSE) PostQuitMessage(0);
		if(uMsg == WM_SIZE)
			test->resize(LOWORD(lParam), HIWORD(lParam));
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

		roVerify(::RegisterClassW(&wc) != 0);

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

		::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(this));
		::ShowWindow(hWnd, true);
	}

	void initContext(const char* driverStr)
	{
		driver = rgNewRenderDriver(driverStr, NULL);
		context = driver->newContext(driver);
		context->magjorVersion = 2;
		context->minorVersion = 0;
		driver->initContext(context, hWnd);
		driver->useContext(context);

		driver->setViewport(0, 0, context->width, context->height, 0, 1);

		vShader = driver->newShader();
		gShader = driver->newShader();
		pShader = driver->newShader();
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

		float elasped = (float)stopWatch.getAndReset();
		averageFrameDuration = elasped/60 + averageFrameDuration * 59.0f / 60;

		printf("\rFPS: %f, frame duration: %fms", 1.0f/averageFrameDuration, averageFrameDuration*1000);

		return true;
	}

	void resize(unsigned width, unsigned height)
	{
		if(!driver) return;
		driver->changeResolution(width, height);
		driver->setViewport(0, 0, context->width, context->height, 0, 1);
	}

	HWND hWnd;
	roRDriver* driver;
	roRDriverContext* context;
	roRDriverShader* vShader;
	roRDriverShader* gShader;
	roRDriverShader* pShader;

	StopWatch stopWatch;
	float averageFrameDuration;
};

const wchar_t* SpriteTest::windowClass = L"Rhinoca unit test";

TEST_FIXTURE(SpriteTest, empty)
{
	createWindow(1, 1);
}

static const unsigned driverIndex = 0;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

TEST_FIXTURE(SpriteTest, uniformBuffer)
{
	static const char* vShaderSrc[] = 
	{
		// GLSL
		"#version 140\n"
		"in vec4 position;"
		"void main(void) { gl_Position = position; }",

		// HLSL
		"float4 main(float4 pos:position):SV_POSITION { return pos; }"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"#version 140\n"
		"uniform color1 { vec4 _color1; };"
		"uniform color2 { vec4 _color2; vec4 _color3; };"
		"out vec4 outColor;"
		"void main(void) { outColor = _color1+_color2+_color3; }",

		// HLSL
		"cbuffer color1 { float4 _color1; }"
		"cbuffer color2 { float4 _color2; float4 _color3; }"
		"float4 main(float4 pos:SV_POSITION):SV_Target { return _color1+_color2+_color3; }"
	};

	// To draw a full screen quad:
	// http://stackoverflow.com/questions/2588875/whats-the-best-way-to-draw-a-fullscreen-quad-in-opengl-3-2

	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	roRDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Create uniform buffer
	float color1[] = { 0, 0, 0, 1 };
	roRDriverBuffer* ubuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer1, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, color1, sizeof(color1)));

	float color2[] = { 0, 0, 0, 1,  0, 0, 0, 1 };
	roRDriverBuffer* ubuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer2, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, color2, sizeof(color2)));

	roRDriverShaderInput shaderInput[] = {
		{ vbuffer, vShader, "position", 0, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 0, 0, 0, 0 },
		{ ubuffer1, pShader, "color1", 0, 0, 0, 0 },
		{ ubuffer2, pShader, "color2", 0, 0, 0, 0 },
	};

	StopWatch stopWatch;

	while(keepRun()) {
		driver->clearColor(0, 0.125f, 0.3f, 1);

		// Animate the color
		color1[0] = (sin(stopWatch.getFloat()) + 1) / 2;
		color2[1] = (cos(stopWatch.getFloat()) + 1) / 2;
		color2[6] = (cos(stopWatch.getFloat() * 2) + 1) / 2;

		driver->updateBuffer(ubuffer1, 0, color1, sizeof(color1));

		float* p = (float*)driver->mapBuffer(ubuffer2, roRDriverBufferMapUsage(roRDriverBufferMapUsage_Write));
		memcpy(p, color2, sizeof(color2));
		driver->unmapBuffer(ubuffer2);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderInput(shaderInput, roCountof(shaderInput), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer1);
	driver->deleteBuffer(ubuffer2);
}