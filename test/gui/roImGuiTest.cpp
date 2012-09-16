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
	createWindow(300, 300);
	initContext(driverStr[driverIndex]);

	Canvas canvas;
	canvas.init();
	canvas.setGlobalColor(1, 0, 0, 1);

	CHECK(guiInit());

	bool showDetails = true;
	GuiPanelState panel[2];
	panel[0].rect = Rectf(0, 0, 500, 400);
	panel[1].rect = Rectf(0, 200, 200-4, 200-4);

	GuiButtonState buttons[3];
	buttons[0].rect = Rectf(5, 100, 90, 30);
	buttons[1].rect = Rectf(105, 100, 90, 30);
	buttons[2].rect = Rectf(5, 180);

	GuiTextAreaState textArea;
	textArea.rect = Rectf(0, 0, 300, 200);

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);

		guiBegin(canvas);
			guiSetTextColor(1, 1, 0, 1);
			guiBeginScrollPanel(panel[0]);

			guiTextArea(textArea, "Hello asdfas dffewar ad eghtdagtewg wdsfg ewrsg hahahahahah\nLine 2\nLine 3");

			guiBeginScrollPanel(panel[1]);
				guiLabel(Rectf(0, 20), "Hello world! I am Ricky Lung");
				guiCheckBox(Rectf(5, 60), "Show details", showDetails);
				if(showDetails) {
					guiButton(buttons[0], "OK");
					guiButton(buttons[1], "Cancel");
					guiButton(buttons[2], "Auto sized");
				}
			guiEndScrollPanel();
			guiEndScrollPanel();
		guiEnd();

		driver->swapBuffers();
	}

	guiClose();
}
