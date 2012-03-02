#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/math/roMatrix.h"

#include <math.h>

using namespace ro;

static float randf() {
	return (float)rand() / RAND_MAX;
}

struct GraphicsDriverTest : public GraphicsTestBase {};

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

TEST_FIXTURE(GraphicsDriverTest, mapBuffer)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	roRDriverDataUsage usage = roRDriverDataUsage_Dynamic;
	float data[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	{	// Simple mapping
		roRDriverBuffer* buffer = driver->newBuffer();
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, usage, data, sizeof(data)));
		float* mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, 0, 0);
		CHECK(mapped);
		CHECK_EQUAL(data[0], mapped[0]);
		driver->unmapBuffer(buffer);
		driver->deleteBuffer(buffer);
	}

	{	// Multiple init
		roRDriverBuffer* buffer = driver->newBuffer();
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, data, sizeof(data)));

		float data2[] = { 10, 20, 30 };
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, data2, sizeof(data2)));

		float* mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, 0, 0);
		CHECK(mapped);
		CHECK_EQUAL(data2[0], mapped[0]);
		driver->unmapBuffer(buffer);
		driver->deleteBuffer(buffer);
	}

	{	// Mapping with offset
		roSize offset = 3;
		roRDriverBuffer* buffer = driver->newBuffer();
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, usage, data, sizeof(data)));
		float* mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, offset * sizeof(float), roCountof(data)-offset);
		CHECK(mapped);
		CHECK_EQUAL(data[offset], mapped[0]);
		driver->unmapBuffer(buffer);

		// Map with read/write mode
		mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_ReadWrite, offset * sizeof(float), roCountof(data)-offset);
		CHECK(mapped);
		mapped[0] += 1;
		driver->unmapBuffer(buffer);

		// Verify the written data
		mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, offset * sizeof(float), roCountof(data)-offset);
		CHECK(mapped);
		CHECK_EQUAL(data[offset] + 1, mapped[0]);
		driver->unmapBuffer(buffer);
		driver->deleteBuffer(buffer);
	}

	{	// Mapping out of range should fail
		roSize offset = 3;
		roRDriverBuffer* buffer = driver->newBuffer();
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, usage, data, sizeof(data)));
		float* mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, offset, 0);
		CHECK(!mapped);
		driver->unmapBuffer(buffer);
		driver->deleteBuffer(buffer);
	}

	{	// Double map should fail
		roRDriverBuffer* buffer = driver->newBuffer();
		CHECK(driver->initBuffer(buffer, roRDriverBufferType_Vertex, usage, data, sizeof(data)));
		float* mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, 0, 0);
		CHECK(mapped);
		mapped = (float*)driver->mapBuffer(buffer, roRDriverMapUsage_Read, 0, 0);
		CHECK(!mapped);
		driver->unmapBuffer(buffer);
		driver->unmapBuffer(buffer);
		driver->deleteBuffer(buffer);
	}
}

TEST_FIXTURE(GraphicsDriverTest, uniformBuffer)
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

	roRDriverShaderBufferInput shaderInput[] = {
		{ vShader, vbuffer, "position", 0, 0, 0, 0 },
		{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
		{ pShader, ubuffer1, "color1", 0, 0, 0, 0 },
		{ pShader, ubuffer2, "color2", 0, 0, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0.125f, 0.3f, 1);

		// Animate the color
		color1[0] = (sin(context->lastSwapTime) + 1) / 2;
		color2[1] = (cos(context->lastSwapTime) + 1) / 2;
		color2[6] = (cos(context->lastSwapTime * 2) + 1) / 2;

		driver->updateBuffer(ubuffer1, 0, color1, sizeof(color1));

		float* p = (float*)driver->mapBuffer(ubuffer2, roRDriverMapUsage(roRDriverMapUsage_Write), 0, 0);
		memcpy(p, color2, sizeof(color2));
		driver->unmapBuffer(ubuffer2);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(shaderInput, roCountof(shaderInput), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer1);
	driver->deleteBuffer(ubuffer2);
}

TEST_FIXTURE(GraphicsDriverTest, textureCommit)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	static const char* vShaderSrc[] = 
	{
		// GLSL
		"in vec4 position;"
		"in vec2 texCoord;"
		"out varying vec2 _texCoord;"
		"void main(void) { gl_Position=position; _texCoord = texCoord; }",

		// HLSL
		"struct VertexInputType { float4 pos : POSITION; float2 texCoord : TEXCOORD0; };"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"PixelInputType main(VertexInputType input) {"
		"	PixelInputType output; output.pos = input.pos; output.texCoord = input.texCoord;"
		"	return output;"
		"}"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform sampler2D tex;"
		"in vec2 _texCoord;"
		"void main(void) { gl_FragColor = texture2D(tex, _texCoord); }",

		// HLSL
		"Texture2D tex;"
		"SamplerState sampleType;"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"float4 main(PixelInputType input):SV_Target {"
		"	return tex.Sample(sampleType, input.texCoord);"
		"}"
	};

	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Init texture
	const unsigned texDim = 1024;
	const unsigned char* texData = (const unsigned char*)malloc(sizeof(char) * 4 * texDim * texDim);

	roRDriverTexture* texture = driver->newTexture();
	CHECK(driver->initTexture(texture, texDim, texDim, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_None, NULL, 0));
	CHECK(driver->commitTexture(texture, texData, 0));

	struct Dummy { static void stallCallback(void*)
	{
	}};
	driver->stallCallback = Dummy::stallCallback;

	// Set the texture state
	roRDriverTextureState textureState =  {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		roRDriverTextureAddressMode_Repeat, roRDriverTextureAddressMode_Repeat
	};

	// Create vertex buffer
	float vertex[][6] = { {-1,1,0,1, 0,1}, {-1,-1,0,1, 0,0}, {1,-1,0,1, 1,0}, {1,1,0,1, 1,1} };
	roRDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Bind shader input layout
	roRDriverShaderBufferInput input[] = {
		{ vShader, vbuffer, "position", 0, 0, sizeof(float)*6, 0 },
		{ vShader, vbuffer, "texCoord", 0, sizeof(float)*4, sizeof(float)*6, 0 },
		{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
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

		//
//		driver->mapTexture(texture, roRDriverMapUsage_Read);
//		driver->unmapTexture(texture);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

		roRDriverShaderTextureInput texInput = { pShader, texture, "tex", stringHash("tex") };
		CHECK(driver->bindShaderTextures(&texInput, 1));
		driver->setTextureState(&textureState, 1, 0);

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
	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc, 1));

	const char* pShaderSrc =
		"uniform sampler2D u_tex;"
		"varying vec2 texCoord;"
		"void main(void){gl_FragColor=texture2D(u_tex, texCoord);}";
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc, 1));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Init texture
	const unsigned char texData[][4] = {
		// a,  b,  g,  r
		{255,254,  0,  0}, {255,  0,255,  0}, {255,  0,  0,255}, {255,255,255,255},		// Bottom row
		{255,254,253,252}, {255,255,  0,  0}, {255,  0,255,  0}, {255,  0,  0,255},		//
		{255,  0,255,255}, {255,255,255,255}, {255,255,  0,  0}, {255,  0,255,  0},		//
		{255,  0,255,  0}, {255,  0,  0,255}, {255,255,255,255}, {255,255,  0,  0}		// Top row
	};
	roRDriverTexture* texture = driver->newTexture();
	CHECK(driver->initTexture(texture, 4, 4, roRDriverTextureFormat_RGBA));
	CHECK(driver->commitTexture(texture, texData, 0));

	// Set the texture state
	roRDriverTextureState textureState =  {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		roRDriverTextureAddressMode_Repeat, roRDriverTextureAddressMode_Repeat
	};

	// Create vertex buffer
	float vertex[][4] = { {-1,1,0,1}, {-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1} };
	roRDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Bind shader input layout
	roRDriverShaderBufferInput input[] = {
		{ vbuffer, vShader,"vertex", 0, 0, 0, 0 },
		{ ibuffer, NULL, NULL, 0, 0, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

		driver->setTextureState(&textureState, 1, 0);
		CHECK(driver->bindShaderTextures(stringHash("u_tex"), texture));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteTexture(texture);
	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
}
*/
TEST_FIXTURE(GraphicsDriverTest, 3d)
{
	static const char* vShaderSrc[] = 
	{
		// GLSL
		"attribute vec3 position;"
		"uniform mat4 modelViewMat;"
		"uniform mat4 projectionMat;"
		"void main(void) { gl_Position = (projectionMat*modelViewMat)*vec4(position,1); }",

		// HLSL
		"cbuffer modelViewMat { float4x4 _modelViewMat; };"
		"cbuffer projectionMat { float4x4 _projectionMat; };"
		"float4 main(float3 pos : position) : SV_POSITION { return mul(mul(_projectionMat,_modelViewMat),float4(pos,1)); }"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform vec4 color;"
		"void main(void) { gl_FragColor = color; }",

		// HLSL
		"cbuffer color { float4 _color; };"
		"float4 main(float4 pos : SV_POSITION) : SV_Target { return _color; }"
	};

	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Create vertex buffer
	float vertex[][3] = { {-1,1,0}, {-1,-1,0}, {1,-1,0}, {1,1,0} };
	roRDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Create uniform buffer
	roRDriverBuffer* ubuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, NULL, sizeof(float)*(16*2+4)));

	// Model view matrix
	Mat4* modelView = (Mat4*)driver->mapBuffer(ubuffer, roRDriverMapUsage_Write, 0, 0);

	float translate[] =  { 0, 0, -3 };
	*modelView = makeTranslationMat4(translate);

	// Projection matrix
	Mat4* prespective = modelView + 1;
	*prespective = makePrespectiveMat4(90, 1, 2, 10);
	driver->adjustDepthRangeMatrix(prespective->data);

	// Color
	float color[] = { 1, 1, 0, 1 };
	memcpy(prespective + 1, color, sizeof(color));

	driver->unmapBuffer(ubuffer);

	// Bind shader input layout
	roRDriverShaderBufferInput input[] = {
		{ vShader, vbuffer, "position", 0, 0, 0, 0 },
		{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
		{ vShader, ubuffer, "modelViewMat", 0, 0, 0, 0 },
		{ vShader, ubuffer, "projectionMat", 0, sizeof(float)*16, 0, 0 },
		{ pShader, ubuffer, "color", 0, sizeof(float)*16*2, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer);
}

TEST_FIXTURE(GraphicsDriverTest, blending)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	static const char* vShaderSrc[] = 
	{
		// GLSL
		"attribute vec4 position;"
		"void main(void) { gl_Position = position; }",

		// HLSL
		"float4 main(float4 pos : position) : SV_POSITION { return pos; }"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform vec4 color;"
		"void main(void) { gl_FragColor = color; }",

		// HLSL
		"cbuffer color { float4 _color; }"
		"float4 main(float4 pos : SV_POSITION) : SV_Target { return _color; }"
	};

	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Create vertex buffer
	float vertex1[][4] = { {-1,1,0,1}, {-1,-0.5f,0,1}, {0.5f,-0.5f,0,1}, {0.5f,1,0,1} };
	roRDriverBuffer* vbuffer1 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer1, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex1, sizeof(vertex1)));

	float vertex2[][4] = { {-0.5f,0.5f,0,1}, {-0.5f,-1,0,1}, {1,-1,0,1}, {1,0.5f,0,1} };
	roRDriverBuffer* vbuffer2 = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer2, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex2, sizeof(vertex2)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Create uniform buffer
	roRDriverBuffer* ubuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, NULL, sizeof(float)*4));

	// Set the blend state
	roRDriverBlendState blend = {
		0, true,
		roRDriverBlendOp_Add, roRDriverBlendOp_Add,
		roRDriverBlendValue_SrcAlpha, roRDriverBlendValue_InvSrcAlpha,
		roRDriverBlendValue_One, roRDriverBlendValue_Zero,
		roRDriverColorWriteMask_EnableAll
	};

	while(keepRun())
	{
		driver->clearColor(0, 0, 0, 0);
		driver->setBlendState(&blend);
		CHECK(driver->bindShaders(shaders, roCountof(shaders)));

		{	// Draw the first quad
			float color[] = { 1, 1, 0, 0.5f };
			driver->updateBuffer(ubuffer, 0, color, sizeof(color));

			roRDriverShaderBufferInput input[] = {
				{ vShader, vbuffer1, "position", 0, 0, 0, 0 },
				{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
				{ pShader, ubuffer, "color", 0, 0, 0, 0 },
			};
			CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		{	// Draw the second quad
			float color[] = { 1, 0, 0, 0.5f };
			driver->updateBuffer(ubuffer, 0, color, sizeof(color));

			roRDriverShaderBufferInput input[] = {
				{ vShader, vbuffer2, "position", 0, 0, 0, 0 },
				{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
				{ pShader, ubuffer, "color", 0, 0, 0, 0 },
			};
			CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

			driver->drawTriangleIndexed(0, 6, 0);
		}

		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer1);
	driver->deleteBuffer(vbuffer2);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer);
}

TEST_FIXTURE(GraphicsDriverTest, GeometryShader)
{
	static const char* vShaderSrc[] = 
	{
		// GLSL
		"attribute vec3 position;"
		"uniform mat4 modelViewMat;"
		"uniform mat4 projectionMat;"
		"void main(void) { gl_Position = (projectionMat*modelViewMat)*vec4(position,1); }",

		// HLSL
		"cbuffer modelViewMat { float4x4 _modelViewMat; };"
		"cbuffer projectionMat { float4x4 _projectionMat; };"
		"float4 main(float3 pos : position) : SV_POSITION { return mul(mul(_projectionMat,_modelViewMat),float4(pos,1)); }"
	};

	static const char* gShaderSrc[] = 
	{
		// GLSL
		"#version 150\n"
		"layout(triangles) in;"
		"layout(triangle_strip, max_vertices=3) out;"
		"void main(void) {"
		"for(int i=0; i<gl_in.length(); ++i) {"
		"	gl_Position = gl_in[i].gl_Position;"
		"	EmitVertex();"
		"} EndPrimitive();"
		"}",

		// HLSL
		"cbuffer color { float4 _color; };"
		"float4 main(float4 pos:SV_POSITION):SV_Target{return _color;}"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform vec4 color;"
		"void main(void){gl_FragColor=color;}",

		// HLSL
		"cbuffer color { float4 _color; };"
		"float4 main(float4 pos:SV_POSITION):SV_Target{return _color;}"
	};

	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	// Init shader
	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(gShader, roRDriverShaderType_Geometry, &gShaderSrc[driverIndex], 1));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	roRDriverShader* shaders[] = { vShader, gShader, pShader };

	// Create vertex buffer
	float vertex[][3] = { {-1,1,0}, {-1,-1,0}, {1,-1,0}, {1,1,0} };
	roRDriverBuffer* vbuffer = driver->newBuffer();
	CHECK(driver->initBuffer(vbuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Static, vertex, sizeof(vertex)));

	// Create index buffer
	rhuint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	roRDriverBuffer* ibuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ibuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Create uniform buffer
	roRDriverBuffer* ubuffer = driver->newBuffer();
	CHECK(driver->initBuffer(ubuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, NULL, sizeof(float)*(16*2+4)));

	// Model view matrix
	Mat4* modelView = (Mat4*)driver->mapBuffer(ubuffer, roRDriverMapUsage_Write, 0, 0);

	float translate[] =  { 0, 0, -3 };
	*modelView = makeTranslationMat4(translate);

	// Projection matrix
	Mat4* prespective = modelView + 1;
	*prespective = makePrespectiveMat4(90, 1, 2, 10);
	driver->adjustDepthRangeMatrix(prespective->data);

	// Color
	float color[] = { 1, 1, 0, 1 };
	memcpy(prespective + 1, color, sizeof(color));

	driver->unmapBuffer(ubuffer);

	// Bind shader input layout
	roRDriverShaderBufferInput input[] = {
		{ vShader, vbuffer, "position", 0, 0, 0, 0 },
		{ NULL, ibuffer, NULL, 0, 0, 0, 0 },
		{ vShader, ubuffer, "modelViewMat", 0, 0, 0, 0 },
		{ vShader, ubuffer, "projectionMat", 0, sizeof(float)*16, 0, 0 },
		{ pShader, ubuffer, "color", 0, sizeof(float)*16*2, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
	driver->deleteBuffer(ubuffer);
}
