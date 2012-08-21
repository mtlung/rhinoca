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

	while(keepRun()) {
		imGuiBegin(canvas);
			imGuiLabel("Hello world!");
		imGuiEnd();
	}
}
