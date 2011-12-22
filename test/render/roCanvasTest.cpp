#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/render/roCanvas.h"
#include "../../roar/render/roTexture.h"

using namespace ro;

namespace ro {

extern Resource* resourceCreateBmp(const char*, ResourceManager*);
extern bool resourceLoadBmp(Resource*, ResourceManager*);

}

struct CanvasTest : public GraphicsTestBase
{
	CanvasTest()
	{
		resourceManager.addFactory(resourceCreateBmp, resourceLoadBmp);
	}

	Canvas canvas;
};

static const unsigned driverIndex = 1;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

TEST_FIXTURE(CanvasTest, drawImage)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);
	canvas.init(context);

	// Init texture
	TexturePtr texture = resourceManager.loadAs<Texture>("EdSplash.bmp");

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);
		canvas.drawImage(texture->handle, 10, 50, 100, 100);
		driver->swapBuffers();
	}

	texture = NULL;
	resourceManager.shutdown();
}
