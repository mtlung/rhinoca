#include "../../roar/render/roRenderDriver.h"
#include "../../roar/base/roResource.h"
#include "../../roar/platform/roPlatformHeaders.h"

struct GraphicsTestBase
{
	static const wchar_t* windowClass;

	GraphicsTestBase();
	~GraphicsTestBase();

	void createWindow(int width, int height);

	void initContext(const char* driverStr);

	bool keepRun();

	void resize(unsigned width, unsigned height);

	HWND hWnd;
	roRDriver* driver;
	roRDriverContext* context;
	roRDriverShader* vShader;
	roRDriverShader* gShader;
	roRDriverShader* pShader;

	float averageFrameDuration;

	ro::TaskPool taskPool;
	ro::ResourceManager resourceManager;
};