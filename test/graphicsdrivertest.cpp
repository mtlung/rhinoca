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

static float randf() {
	return (float)rand() / RAND_MAX;
}

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
		GraphicsDriverTest* test = reinterpret_cast<GraphicsDriverTest*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
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

	void resize(unsigned width, unsigned height)
	{
		driver->changeResolution(width, height);
		driver->setViewport(0, 0, context->width, context->height, 0, 1);
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

static const unsigned driverIndex = 1;

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

	// HLSL
	"cbuffer color1 { float4 _color1; }"
	"cbuffer color2 { float4 _color2; float4 _color3; }"
	"float4 main(float4 pos:SV_POSITION):SV_Target{return _color1+_color2+_color3;}"
};

TEST_FIXTURE(GraphicsDriverTest, uniformBuffer)
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
	CHECK(driver->initBuffer(vbuffer, RgDriverBufferType_Vertex, RgDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	RgDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, RgDriverBufferType_Index, RgDriverDataUsage_Static, index, sizeof(index)));

	// Create uniform buffer
	float color1[] = { 0, 0, 0, 1 };
	RgDriverBuffer* ubuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer1, RgDriverBufferType_Uniform, RgDriverDataUsage_Dynamic, color1, sizeof(color1)));

	float color2[] = { 0, 0, 0, 1,  0, 0, 0, 1 };
	RgDriverBuffer* ubuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer2, RgDriverBufferType_Uniform, RgDriverDataUsage_Dynamic, color2, sizeof(color2)));

	RgDriverShaderInput shaderInput[] = {
		{ vbuffer, vShader, "position", 0, 4, 0, 4*sizeof(float), 0 },
		{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
		{ ubuffer1, pShader, "color1", 0, 1, 0, 0, 0 },
		{ ubuffer2, pShader, "color2", 0, 1, 0, 0, 0 },
	};

	Timer timer;

	while(keepRun()) {
		driver->clearColor(0, 0.125f, 0.3f, 1);

		// Animate the color
		color1[0] = (sin(timer.seconds()) + 1) / 2;
		color2[1] = (cos(timer.seconds()) + 1) / 2;
		color2[6] = (cos(timer.seconds() * 2) + 1) / 2;

		driver->updateBuffer(ubuffer1, 0, color1, sizeof(color1));

		float* p = (float*)driver->mapBuffer(ubuffer2, RgDriverBufferMapUsage(RgDriverBufferMapUsage_Write));
		memcpy(p, color2, sizeof(color2));
		driver->unmapBuffer(ubuffer2);

		CHECK(driver->bindShaders(shaders, 2));
		CHECK(driver->bindShaderInput(shaderInput, COUNTOF(shaderInput), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer1);
	driver->deleteBuffer(ubuffer2);
}

TEST_FIXTURE(GraphicsDriverTest, textureCommit)
{return;
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

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
	const unsigned texDim = 1024;
	const unsigned char* texData = (const unsigned char*)malloc(sizeof(char) * 4 * texDim * texDim);

	RgDriverTexture* texture = driver->newTexture();
	CHECK(driver->initTexture(texture, texDim, texDim, RgDriverTextureFormat_RGBA));
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
		{ vbuffer, vShader,"vertex", 0, 4, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		// Make some noise in the texture
		for(unsigned i=texDim; i--;) {
			unsigned idx = unsigned(randf() * (texDim * texDim));
			unsigned* pixel = ((unsigned*)texData) + idx % (texDim * texDim);
			*pixel = unsigned(randf() * UINT_MAX);
			pixel = pixel;
		}
		CHECK(driver->commitTexture(texture, texData, 0));

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
	free((void*)texData);
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
		{ vbuffer, vShader,"vertex", 0, 4, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
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
*/
TEST_FIXTURE(GraphicsDriverTest, 3d)
{return;
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

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

	// Create uniform buffer
	RgDriverBuffer* ubuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer, RgDriverBufferType_Uniform, RgDriverDataUsage_Stream, NULL, sizeof(float)*(16*2+4)));

	// Model view matrix
	float* modelView = (float*)driver->mapBuffer(ubuffer, RgDriverBufferMapUsage_Write);

	rgMat44MakeIdentity(modelView);
	float translate[] =  { 0, 0, -3 };
	rgMat44TranslateBy(modelView, translate);

	// Projection matrix
	float* prespective = modelView + 16;
	rgMat44MakePrespective(prespective, 90, 1, 2, 10);

	// Color
	float color[] = { 1, 1, 0, 1 };
	memcpy(prespective + 16, color, sizeof(color));

	driver->unmapBuffer(ubuffer);

	// Bind shader input layout
	RgDriverShaderInput input[] = {
		{ vbuffer, vShader, "vertex", 0, 3, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
		{ ubuffer, pShader, "modelViewMat", 0, 1, 0, 0, 0 },
		{ ubuffer, pShader, "projectionMat", 0, 1, sizeof(float)*16, 0, 0 },
		{ ubuffer, pShader, "u_color", 0, 1, sizeof(float)*16*2, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, 2));
		CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer);
}

TEST_FIXTURE(GraphicsDriverTest, blending)
{return;
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

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

	// Create uniform buffer
	RgDriverBuffer* ubuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer, RgDriverBufferType_Uniform, RgDriverDataUsage_Stream, NULL, sizeof(float)*4));

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
			float color[] = { 1, 1, 0, 0.5f };
			driver->updateBuffer(ubuffer, 0, color, sizeof(color));

			RgDriverShaderInput input[] = {
				{ vbuffer1, vShader, "vertex", 0, 4, 0, 0, 0 },
				{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
				{ ubuffer, pShader, "u_color", 0, 1, 0, 0, 0 },
			};
			CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		{	// Draw the second quad
			float color[] = { 1, 0, 0, 0.5f };
			driver->updateBuffer(ubuffer, 0, color, sizeof(color));

			RgDriverShaderInput input[] = {
				{ vbuffer2, vShader, "vertex", 0, 4, 0, 0, 0 },
				{ ibuffer, NULL, NULL, 0, 1, 0, 0, 0 },
				{ ubuffer, pShader, "u_color", 0, 1, 0, 0, 0 },
			};
			CHECK(driver->bindShaderInput(input, COUNTOF(input), NULL));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer1);
	driver->deleteBuffer(vbuffer2);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer);
}
