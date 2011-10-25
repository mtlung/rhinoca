#include "pch.h"
#include "../src/render/driver2.h"
#include "../src/render/rgutility.h"
#include "../src/rhassert.h"
#include "../src/rhstring.h"
#include "../src/timer.h"

#include <math.h>

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
			driver->deleteShader(vShader);
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
	}

	void initContext(const char* driverStr)
	{
		driver = rgNewRenderDriver(driverStr, NULL);
		context = driver->newContext(driver);
		context->magjorVersion = 3;
		context->minorVersion = 2;
		driver->initContext(context, hWnd);
		driver->useContext(context);

		driver->setViewport(0, 0, context->width, context->height, 0, 1);

		vShader = driver->newShader();
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

		return true;
	}

	HWND hWnd;
	RgDriver* driver;
	RgDriverContext* context;
	RgDriverShader* vShader;
	RgDriverShader* pShader;
};

const wchar_t* GraphicsDriverTest::windowClass = L"Rhinoca unit test";

TEST_FIXTURE(GraphicsDriverTest, empty)
{
	createWindow(1, 1);
}

static const unsigned driverIndex = 0;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

static const char* vShaderSrc[] = 
{
	// GLSL
	"#version 140\n"
	"in vec4 position;"
	"void main(void){gl_Position=position;}",

	// HLSL
	"float4 main(float4 pos:position):SV_POSITION{return pos;}"
};

static const char* pShaderSrc[] = 
{
	// GLSL
	"#version 140\n"
	"uniform color1 { vec4 _color1; };"
	"uniform color2 { vec4 _color2; vec4 _color3; };"
	"out vec4 outColor;"
	"void main(void){outColor=_color1+_color2+_color3;}",

	"cbuffer color1 { float4 _color1; }"
	"cbuffer color2 { float4 _color2; float4 _color3; }"
	"float4 main(float4 pos:SV_POSITION):SV_Target{return _color1+_color2+_color3;}"
};

TEST_FIXTURE(GraphicsDriverTest, glUniformBuffer)
{
	// To draw a full screen quad:
	// http://stackoverflow.com/questions/2588875/whats-the-best-way-to-draw-a-fullscreen-quad-in-opengl-3-2

	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	CHECK(driver->initShader(vShader, RgDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, RgDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	RgDriverShader* shaders[] = { vShader, pShader };

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	RgDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, RgDriverDataUsage_Stream, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, RgDriverDataUsage_Static, index, sizeof(index)));

	RgDriverShaderInput shaderInput[] = {
		{ vbuffer, vShader, "position", 4, 0, 4*sizeof(float), 0, 0 },
		{ ibuffer, NULL, NULL, 1, 0, 0, 0, 0 },
	};

	// Create uniform buffer
	float color1[] = { 0, 0, 0, 1 };
	RgDriverBuffer* ubuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer1, RgDriverBufferType_Uniform, RgDriverDataUsage_Dynamic, color1, sizeof(color1)));

	float color2[] = { 0, 0, 0, 1,  0, 0, 0, 1 };
	RgDriverBuffer* ubuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer2, RgDriverBufferType_Uniform, RgDriverDataUsage_Dynamic, color2, sizeof(color2)));

	Timer timer;

	while(keepRun()) {
		driver->clearColor(0, 0.125f, 0.3f, 1);

		CHECK(driver->bindShaders(shaders, 2));
		CHECK(driver->bindShaderInput(shaderInput, COUNTOF(shaderInput), NULL));

		// Animate the color
		color1[0] = (sin(timer.seconds()) + 1) / 2;
		color2[1] = (cos(timer.seconds()) + 1) / 2;
		color2[6] = (cos(timer.seconds() * 2) + 1) / 2;

		float* p = (float*)driver->mapBuffer(ubuffer1, RgDriverBufferMapUsage(RgDriverBufferMapUsage_Write));
		memcpy(p, color1, sizeof(color1));
		driver->unmapBuffer(ubuffer1);

		p = (float*)driver->mapBuffer(ubuffer2, RgDriverBufferMapUsage(RgDriverBufferMapUsage_Write));
		memcpy(p, color2, sizeof(color2));
		driver->unmapBuffer(ubuffer2);

		CHECK(driver->setUniformBuffer(StringHash("color1"), ubuffer1));
		CHECK(driver->setUniformBuffer(StringHash("color2"), ubuffer2));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(ubuffer1);
	driver->deleteBuffer(ubuffer2);
	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
}
/*
TEST_FIXTURE(GraphicsDriverTest, _texture)
{
	createWindow(200, 200);

	// Init shader
	const char* vShaderSrc =
		"attribute vec4 vertex;"
		"varying vec2 texCoord;"
		"void main(void){texCoord=(vertex+1)/2;gl_Position=vertex;}";
	CHECK(driver->initShader(vShader, RgDriverShaderType_Vertex, &vShaderSrc, 1));

	const char* pShaderSrc =
		"uniform sampler2D u_tex;"
		"varying vec2 texCoord;"
		"void main(void){gl_FragColor=texture2D(u_tex, texCoord);}";
	CHECK(driver->initShader(pShader, RgDriverShaderType_Pixel, &pShaderSrc, 1));

	RgDriverShader* shaders[] = { vShader, pShader };

	// Init texture
	const unsigned char texData[][4] = {
		// a,  b,  g,  r
		{255,254,  0,  0}, {255,  0,255,  0}, {255,  0,  0,255}, {255,255,255,255},		// Bottom row
		{255,254,253,252}, {255,255,  0,  0}, {255,  0,255,  0}, {255,  0,  0,255},		//
		{255,  0,255,255}, {255,255,255,255}, {255,255,  0,  0}, {255,  0,255,  0},		//
		{255,  0,255,  0}, {255,  0,  0,255}, {255,255,255,255}, {255,255,  0,  0}		// Top row
	};
	RgDriverTexture* texture = driver->newTexture();
	CHECK(driver->initTexture(texture, 4, 4, RgDriverTextureFormat_RGBA));
	CHECK(driver->commitTexture(texture, texData, 0));

	// Set the texture state
	RgDriverTextureState textureState =  {
		0,
		RgDriverTextureFilterMode_MinMagLinear,
		RgDriverTextureAddressMode_Repeat, RgDriverTextureAddressMode_Repeat
	};

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	RgDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, RgDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, RgDriverDataUsage_Static, index, sizeof(index)));

	// Bind shader input layout
	RgDriverShaderInput input[] = {
		{ vbuffer, vShader,"vertex", 4, 0, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 1, 0, 0, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, 2));
		CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

		driver->setTextureState(&textureState, 1, 0);
		CHECK(driver->setUniformTexture(StringHash("u_tex"), texture));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteTexture(texture);
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

	// Create vertex buffer
	float vertex[][3] = { {-1,1,0}, {-1,-1,0}, {1,-1,0}, {1,1,0} };
	RgDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, RgDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, RgDriverDataUsage_Static, index, sizeof(index)));

	// Bind shader input layout
	RgDriverShaderInput input[] = {
		{ vbuffer, vShader, "vertex", 3, 0, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 1, 0, 0, 0, 0 },
	};

	// Model view matrix
	float modelView[16];
	rgMat44MakeIdentity(modelView);
	float translate[] =  { 0, 0, -3 };
	rgMat44TranslateBy(modelView, translate);

	// Projection matrix
	float prespective[16];
	rgMat44MakePrespective(prespective, 90, 1, 2, 10);

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, 2));
		CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

		CHECK(driver->setUniformMat44fv(StringHash("modelViewMat"), false, modelView, 1));
		CHECK(driver->setUniformMat44fv(StringHash("projectionMat"), false, prespective, 1));

		float c[] = { 1, 1, 0, 1 };
		CHECK(driver->setUniform4fv(StringHash("u_color"), c, 1));

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

	// Create vertex buffer
	float vertex1[][4] = { {-1,1,0,1}, {-1,-0.5f,0,1}, {0.5f,-0.5f,0,1}, {0.5f,1,0,1} };
	RgDriverBuffer* vbuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer1, RgDriverBufferType_Vertex, RgDriverDataUsage_Static, vertex1, sizeof(vertex1)));

	float vertex2[][4] = { {-0.5f,0.5f,0,1}, {-0.5f,-1,0,1}, {1,-1,0,1}, {1,0.5f,0,1} };
	RgDriverBuffer* vbuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer2, RgDriverBufferType_Vertex, RgDriverDataUsage_Static, vertex2, sizeof(vertex2)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, RgDriverDataUsage_Static, index, sizeof(index)));

	// Set the blend state
	RgDriverBlendState blend = {
		0, true,
		RgDriverBlendOp_Add, RgDriverBlendOp_Add,
		RgDriverBlendValue_SrcAlpha, RgDriverBlendValue_InvSrcAlpha,
		RgDriverBlendValue_One, RgDriverBlendValue_Zero,
		RgDriverColorWriteMask_EnableAll
	};

	while(keepRun())
	{
		driver->clearColor(0, 0, 0, 0);
		driver->setBlendState(&blend);
		CHECK(driver->bindShaders(shaders, 2));

		{	// Draw the first quad
			RgDriverShaderInput input[] = {
				{ vbuffer1, vShader, "vertex", 4, 0, 0, 0, 0 },
				{ ibuffer, NULL, NULL, 1, 0, 0, 0, 0 },
			};
			CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

			float c[] = { 1, 1, 0, 0.5f };
			CHECK(driver->setUniform4fv(StringHash("u_color"), c, 1));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		{	// Draw the second quad
			RgDriverShaderInput input[] = {
				{ vbuffer2, vShader, "vertex", 4, 0, 0, 0, 0 },
				{ ibuffer, NULL, NULL, 1, 0, 0, 0, 0 },
			};
			CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

			float c[] = { 1, 0, 0, 0.5f };
			CHECK(driver->setUniform4fv(StringHash("u_color"), c, 1));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer1);
	driver->deleteBuffer(vbuffer2);
	driver->deleteBuffer(ibuffer);
}
*/