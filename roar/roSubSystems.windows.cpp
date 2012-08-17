#include "pch.h"
#include "roSubSystems.h"
#include "input/roInputDriver.h"
#include "render/roRenderDriver.h"
#include "platform/roPlatformHeaders.h"

namespace ro {

void SubSystems::processEvents(void** data, roSize numData)
{
	HWND hWnd = HWND(data[0]);
	UINT uMsg = UINT(data[1]);
	WPARAM wParam = WPARAM(data[2]);
	LPARAM lParam = LPARAM(data[3]);

	(void)hWnd;
	(void)wParam;

	if(uMsg == WM_SIZE && renderDriver && renderContext) {
		roVerify(renderDriver->changeResolution(LOWORD(lParam), HIWORD(lParam)));
		renderDriver->setViewport(0, 0, renderContext->width, renderContext->height, 0, 1);
	}

	if(inputDriver)
		inputDriver->processEvents(inputDriver, data, numData);
}

}	// namespace ro
