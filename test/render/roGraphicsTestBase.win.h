#include "../../roar/render/roRenderDriver.h"
#include "../../roar/platform/roPlatformHeaders.h"
#include "../../roar/roSubSystems.h"

struct GraphicsTestBase
{
	static const wchar_t* windowClass;

	GraphicsTestBase();
	~GraphicsTestBase();

	void createWindow(int width, int height);

	ro::Status initContext(const char* driverStr);

	bool keepRun();

	void resize(unsigned width, unsigned height);

	HWND hWnd;
	roRDriver* driver;
	roRDriverContext* context;
	roRDriverShader* vShader;
	roRDriverShader* gShader;
	roRDriverShader* pShader;

	float averageFrameDuration;
	float maxFrameDuration;

	ro::SubSystems subSystems;
};