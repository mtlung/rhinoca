#include "pch.h"
#include "../render/roGraphicsTestBase.win.h"
#include "../../roar/gui/roGui.h"
#include "../../roar/base/roStringFormat.h"
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
	CHECK(guiInit());

	bool showDetails = true;
	GuiPanelState panel[2];
	panel[0].rect = Rectf(0, 0, 500, 400);
	panel[1].rect = Rectf(0, 250, 200-4, 200-4);

	GuiButtonState buttons[3];
	buttons[0].rect = Rectf(5, 100, 90, 30);
	buttons[1].rect = Rectf(105, 100, 90, 30);
	buttons[2].rect = Rectf(5, 180);

	GuiTextAreaState textArea;
	textArea.rect = Rectf(0, 0, 300, 200);

	GuiTabAreaState tabArea;
	String text = "Hello asdfas dffewar ad eghtdagtewg wdsfg ewrsg hahahahahah\nLine 2\nLine 3";

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			tabArea.rect.w = (float)canvas.width();
			tabArea.rect.h = (float)canvas.height();
//			guiBeginTabs(tabArea);
			guiBeginScrollPanel(panel[0]);

			guiTextArea(textArea, text);

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
//			guiEndTabs();
		guiEnd();
		driver->swapBuffers();
	}

	guiClose();
}

TEST_FIXTURE(ImGuiTest, autoSizedPanel)
{
	createWindow(380, 120);
	initContext(driverStr[driverIndex]);
	Canvas canvas;
	canvas.init();
	CHECK(guiInit());

	bool autoSizedButton = false;
	GuiPanelState panel[2];

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			GuiButtonState button;
			if(!autoSizedButton)
				button.rect.setSize(100, 100);

			guiBeginScrollPanel(panel[0]);
			guiBeginScrollPanel(panel[1]);
				guiButton(button, "This is a button");
			guiEndScrollPanel();
			guiEndScrollPanel();

			guiCheckBox(Rectf(120, 0), "Button auto size", autoSizedButton);

			String str;
			strFormat(str, "Outer most panel: width-{}, height-{}", panel[0].deducedRect.w, panel[0].deducedRect.h);
			guiLabel(Rectf(120, 40), str.c_str());
		guiEnd();
		driver->swapBuffers();
	}

	guiClose();
}

TEST_FIXTURE(ImGuiTest, flowLayout)
{
	createWindow(300, 300);
	initContext(driverStr[driverIndex]);
	Canvas canvas;
	canvas.init();
	CHECK(guiInit());

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			GuiButtonState button;

			// Horizontal flow layout
			guiBeginFlowLayout(Rectf(10, 10, 0, 30), 'h');
				guiButton(button, "Horizontal");
				guiButton(button, "flow");
				guiButton(button, "layout");
			guiEndFlowLayout();

			// Vertical flow layout
			guiBeginFlowLayout(Rectf(10, 50, 60, 0), 'v');
				guiButton(button, "Vertical");
				guiButton(button, "flow");
				guiButton(button, "layout");
			guiEndFlowLayout();

			// Horizontal flow layout inside vertical flow layout
			guiBeginFlowLayout(Rectf(100, 50, 0, 0), 'v');
				guiBeginFlowLayout(Rectf(0, 0, 0, 30), 'h');
					guiButton(button, "Two");
					guiButton(button, "horizontal");
					guiButton(button, "layout");
				guiEndFlowLayout();
				guiBeginFlowLayout(Rectf(0, 0, 0, 30), 'h');
					guiButton(button, "in");
					guiButton(button, "a");
					guiButton(button, "vertical");
					guiButton(button, "layout");
				guiEndFlowLayout();
			guiEndFlowLayout();
		guiEnd();
		driver->swapBuffers();
	}

	guiClose();
}

TEST_FIXTURE(ImGuiTest, tabPanel)
{
	createWindow(300, 300);
	initContext(driverStr[driverIndex]);
	Canvas canvas;
	canvas.init();
	CHECK(guiInit());

	GuiTabAreaState tabArea;
	GuiButtonState button;

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			tabArea.rect.w = (float)canvas.width();
			tabArea.rect.h = (float)canvas.height();

			GuiButtonState button;

			guiBeginTabs(tabArea);
				if(guiBeginTab("Tab 1"))
					guiButton(button, "This is a button in Tab 1");
				guiEndTab();
				if(guiBeginTab("Tab 2"))
					guiButton(button, "This is a button in Tab 2");
				guiEndTab();
				if(guiBeginTab("Tab 3"))
					guiButton(button, "This is a button in Tab 3");
				guiEndTab();
			guiEndTabs();
		guiEnd();
		driver->swapBuffers();
	}

	guiClose();
}

TEST_FIXTURE(ImGuiTest, textArea)
{
	createWindow(300, 300);
	initContext(driverStr[driverIndex]);
	Canvas canvas;
	canvas.init();
	CHECK(guiInit());

	GuiTextAreaState textArea;
	String text = "Hello";

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			textArea.rect.w = (float)canvas.width();
			textArea.rect.h = (float)canvas.height();
			guiTextArea(textArea, text);
		guiEnd();
		driver->swapBuffers();
	}

	guiClose();
}

struct MyWindow : GuiWindowState
{
	GuiButtonState button;
};

static void showDebugLabel()
{
	String str;
	strFormat(str, "Mouse pos {}, {}", guiMousePos().x, guiMousePos().y);
	guiLabel(Rectf(0, 0), str.c_str());

	str.clear();
	strFormat(str, "Mouse down pos {}, {}", guiMouseDownPos().x, guiMouseDownPos().y);
	guiLabel(Rectf(0, 20), str.c_str());
}

static void drawWindow1(GuiWindowState& state)
{
	MyWindow& window = static_cast<MyWindow&>(state);

	showDebugLabel();

	window.button.rect = Rectf(20, 50);
	guiButton(window.button, "Button");
}

static void drawWindow2(GuiWindowState& state)
{
	showDebugLabel();
}

static void drawWindow3(GuiWindowState& state)
{
	showDebugLabel();
}

TEST_FIXTURE(ImGuiTest, window)
{
	createWindow(500, 500);
	initContext(driverStr[driverIndex]);
	Canvas canvas;
	canvas.init();
	CHECK(guiInit());

	GuiButtonState button;

	MyWindow window1;
	window1.title = "Window 1 ...";
	window1.rect = Rectf(50, 40, 200, 100);
	window1.windowFunction = drawWindow1;

	GuiWindowState window2;
	window2.title = "Window 2 ...";
	window2.rect = Rectf(100, 80, 200, 100);
	window2.windowFunction = drawWindow2;

	GuiWindowState window3;
	window3.title = "Window 3 ...";
	window3.rect = Rectf(150, 120, 200, 100);
	window3.windowFunction = drawWindow3;

	while(keepRun()) {
		driver->clearColor(68.0f/256, 68.0f/256, 68.0f/256, 1);
		guiBegin(canvas);
			showDebugLabel();

			guiButton(button, "Button");

			guiWindow(window1);
			guiWindow(window2);
			guiWindow(window3);
		guiEnd();

		driver->swapBuffers();
	}

	guiClose();
}
