#include "pch.h"
#include "../render/roGraphicsTestBase.win.h"
#include "../../roar/gui/roImGui.h"
#include "../../roar/render/roCanvas.h"

using namespace ro;

struct ImGuiTest : public GraphicsTestBase
{
};

static const unsigned driverIndex = 1;

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

	CHECK(imGuiInit());

	bool showDetails = false;

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);

		imGuiBegin(canvas);
			imGuiSetTextColor(1, 1, 0, 1);
			imGuiLabel(imGuiRect(0, 20), "Hello world! I am Ricky Lung");
			imGuiCheckBox(imGuiRect(5, 60), "Show details", showDetails);
			if(showDetails) {
				imGuiButton(imGuiRect(5, 100, 90, 30), "OK");
				imGuiButton(imGuiRect(105, 100, 90, 30), "Cancel");
				imGuiButton(imGuiRect(5, 150), "Auto sized");
			}
		imGuiEnd();

		driver->swapBuffers();
	}

	imGuiClose();
}

/*

*/
