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

static const unsigned driverIndex = 0;

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
	TexturePtr texture = resourceManager.loadAs<Texture>("gogo1.bmp");

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);
		canvas.beginDraw();
		canvas.drawImage(texture->handle, 10, 50, 100, 100);
		canvas.endDraw();
		driver->swapBuffers();
	}

	texture = NULL;
	resourceManager.shutdown();
}

TEST_FIXTURE(CanvasTest, drawToCanvas)
{
	createWindow(400, 400);
	initContext(driverStr[driverIndex]);
	canvas.init(context);

	// Initialize our canvas which use it's own texture as render target
	Canvas auxCanvas;
	auxCanvas.init(context);
	auxCanvas.initTargetTexture(400, 400);

	// Init texture
	TexturePtr texture = resourceManager.loadAs<Texture>("gogo1.bmp");

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);

		// Draw to auxCanvas
		auxCanvas.beginDraw();
		driver->clearColor(1, 0, 0, 0);
		auxCanvas.drawImage(texture->handle, 10, 50, 200, 200);
		auxCanvas.endDraw();

		// Draw to the main canvas
		canvas.beginDraw();
		canvas.drawImage(auxCanvas.targetTexture->handle, 0, 0);
		canvas.endDraw();

		driver->swapBuffers();
	}

	texture = NULL;
	resourceManager.shutdown();
}