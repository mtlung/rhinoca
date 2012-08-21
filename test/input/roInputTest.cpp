#include "pch.h"
#include "../render/roGraphicsTestBase.win.h"
#include "../../roar/input/roInputDriver.h"
#include "../../roar/base/roStringHash.h"
#include "../../roar/roSubSystems.h"

using namespace ro;

struct InputTest : GraphicsTestBase
{
};

TEST_FIXTURE(InputTest, mouse)
{
	createWindow(200, 200);
	subSystems.init();
	subSystems.enableCpuProfiler(false);

	roInputDriver* driver = subSystems.inputDriver;

	float mouseX = 0, mouseY = 0;

	while(keepRun()) {
		float x = driver->mouseAxis(driver, stringHash("mouse x"));
		float y = driver->mouseAxis(driver, stringHash("mouse y"));

		if(x != mouseX || y != mouseY) {
			printf("%f, %f\n", mouseX, mouseY);
			mouseX = x;
			mouseY = y;
		}

		if(driver->anyButtonDown(driver, false))
			printf("Any button\n");
	}
}
