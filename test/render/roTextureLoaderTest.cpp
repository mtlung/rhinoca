#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/render/roTexture.h"
#include "../../roar/base/roFileSystem.h"
#include "../../roar/base/roCpuProfiler.h"
#include "../../roar/render/roCanvas.h"

using namespace ro;

struct TextureLoaderTest : public GraphicsTestBase
{
};

static const unsigned driverIndex = 0;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

TEST_FIXTURE(TextureLoaderTest, load)
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
		"void main(void) { gl_Position=position; texCoord.y = 1-texCoord.y; _texCoord = texCoord; }",

		// HLSL
		"struct VertexInputType { float4 pos : POSITION; float2 texCoord : TEXCOORD0; };"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"PixelInputType main(VertexInputType input) {"
		"	PixelInputType output; output.pos = input.pos; input.texCoord.y = 1-input.texCoord.y; output.texCoord = input.texCoord; "
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

	CHECK(driver->initShader(vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1, NULL, NULL));
	CHECK(driver->initShader(pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1, NULL, NULL));

	roRDriverShader* shaders[] = { vShader, pShader };

	// Init texture
	TexturePtr texture = subSystems.resourceMgr->loadAs<Texture>("gogo1.bmp");

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
		{ vbuffer, "position", 0, roRDriverBufferFormatType_Auto, 0, sizeof(float)*6, 0 },
		{ vbuffer, "texCoord", 0, roRDriverBufferFormatType_Auto, sizeof(float)*4, sizeof(float)*6, 0 },
		{ ibuffer, NULL, 0, roRDriverBufferFormatType_Auto, 0, 0, 0 },
	};

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		CHECK(driver->bindShaders(shaders, roCountof(shaders)));
		CHECK(driver->bindShaderBuffers(input, roCountof(input), NULL));

		roRDriverShaderTextureInput texInput = { pShader, texture->handle, "tex", stringHash("tex") };
		CHECK(driver->bindShaderTextures(&texInput, 1));
		driver->setTextureState(&textureState, 1, 0);

		driver->drawTriangleIndexed(0, 6, 0);
		driver->swapBuffers();
	}

	texture = NULL;

	driver->deleteBuffer(vbuffer);
	driver->deleteBuffer(ibuffer);
}

TEST_FIXTURE(TextureLoaderTest, stressTest)
{
	createWindow(800, 600);
	initContext(driverStr[driverIndex]);
	subSystems.enableCpuProfiler(true);

	Canvas canvas;
	canvas.init();

	Array<ResourcePtr> resources;

	String rootPath = "D:\\jpegs\\";
	void* dir = ro::fileSystem.openDir(rootPath.c_str());
	CHECK("Open directory failed" && dir);

	if(!dir)
		return;

	bool allListed = false;

	roScopeProfile(__FUNCTION__);

	while(keepRun()) {
		roScopeProfile("Main loop");

		// Check if any of the texture are loaded
		for(ResourcePtr& i : resources) {
			TexturePtr tex = dynamic_cast<Texture*>(i.get());
			if(!tex)
				continue;
			if(i->state == Resource::Loaded) {
				canvas.drawImage(tex->handle,
					0, 0, (float)tex->width(), (float)tex->height(),
					0, 0, (float)context->width, (float)context->height);
				resources.removeBySwap(i);
				driver->swapBuffers();
				subSystems.resourceMgr->collectInfrequentlyUsed();
				break;
			}
		}

		if(allListed)
			continue;

		const roUtf8* fileName = ro::fileSystem.dirName(dir);
		if(!fileName)
			break;

		String path = rootPath;
		path += fileName;

		resources.pushBack(subSystems.resourceMgr->load(path.c_str()));
		if(!ro::fileSystem.nextDir(dir))
			allListed = true;
	}

	ro::fileSystem.closeDir(dir);
}
