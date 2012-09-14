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

	CHECK(imGuiInit());

	bool showDetails = true;
	imGuiPanelState panel[2];
	panel[0].rect = imGuiRect(0, 0, 500, 400);
	panel[1].rect = imGuiRect(0, 200, 200-4, 200-4);

	imGuiButtonState buttons[3];
	buttons[0].rect = imGuiRect(5, 100, 90, 30);
	buttons[1].rect = imGuiRect(105, 100, 90, 30);
	buttons[2].rect = imGuiRect(5, 180);

	imGuiTextAreaState textArea;
	textArea.rect = imGuiRect(0, 0, 300, 200);

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);

		imGuiBegin(canvas);
			imGuiSetTextColor(1, 1, 0, 1);
			imGuiBeginScrollPanel(panel[0]);

			imGuiTextArea(textArea, "Hello asdfas dffewar ad eghtdagtewg wdsfg ewrsg hahahahahah\nLine 2\nLine 3");

			imGuiBeginScrollPanel(panel[1]);
				imGuiLabel(imGuiRect(0, 20), "Hello world! I am Ricky Lung");
				imGuiCheckBox(imGuiRect(5, 60), "Show details", showDetails);
				if(showDetails) {
					imGuiButton(buttons[0], "OK");
					imGuiButton(buttons[1], "Cancel");
					imGuiButton(buttons[2], "Auto sized");
				}
			imGuiEndScrollPanel();
			imGuiEndScrollPanel();
		imGuiEnd();

		driver->swapBuffers();
	}

	imGuiClose();
}
