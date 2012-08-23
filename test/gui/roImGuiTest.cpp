#include "pch.h"
#include "../render/roGraphicsTestBase.win.h"
#include "../../roar/gui/roImGui.h"
#include "../../roar/render/roCanvas.h"

using namespace ro;

struct ImGuiTest : public GraphicsTestBase
{
};

static const unsigned driverIndex = 0;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

TEST_FIXTURE(ImGuiTest, button)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);

	Canvas canvas;
	canvas.init();
	canvas.setGlobalColor(1, 0, 0, 1);

	while(keepRun()) {
		driver->clearColor(0, 0, 0, 0);
		canvas.clearRect(0, 0, (float)context->width, (float)context->height);

		imGuiBegin(canvas);
			imGuiSetTextColor(1, 1, 0, 1);
			imGuiLabel(imGuiRect(0, 40), "Hello world! I am Ricky Lung");
		imGuiEnd();

		driver->swapBuffers();
	}
}

/*

*/
