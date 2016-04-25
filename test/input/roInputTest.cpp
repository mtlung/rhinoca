#include "pch.h"
#include "../render/roGraphicsTestBase.win.h"
#include "../../roar/input/roInputDriver.h"
#include "../../roar/base/roStringHash.h"

using namespace ro;

struct InputTest : GraphicsTestBase
{
};

TEST_FIXTURE(InputTest, mouse)
{
	createWindow(200, 200);
	subSystems.init();

	roInputDriver* inputDriver = subSystems.inputDriver;

	float mouseX = 0, mouseY = 0;

	while(keepRun()) {
		float x = inputDriver->mouseAxis(inputDriver, stringHash("mouse x"));
		float y = inputDriver->mouseAxis(inputDriver, stringHash("mouse y"));

		if(x != mouseX || y != mouseY) {
			roLog("", "%f, %f\n", mouseX, mouseY);
			mouseX = x;
			mouseY = y;
		}

		if(inputDriver->anyButtonDown(inputDriver, false))
			roLog("", "Any button\n");
	}
}
