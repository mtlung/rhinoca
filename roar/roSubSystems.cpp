#include "pch.h"
#include "roSubSystems.h"
#include "render/roFont.h"
#include "base/roMemoryProfiler.h"
#include "base/roResource.h"
#include "base/roSocket.h"
#include "base/roTaskPool.h"
#include "render/roRenderDriver.h"

namespace ro {

static void _initTaskPool(SubSystems& subSystems)
{
	subSystems.taskPool = new TaskPool;
	subSystems.taskPool->init(2);
}

static void _initRenderDriver(SubSystems& subSystems)
{
	// Left for the application to initialize, because it need platform specific handle
}

static void _initResourceManager(SubSystems& subSystems)
{
	subSystems.resourceMgr = new ResourceManager;
	subSystems.resourceMgr->taskPool = subSystems.taskPool;

	extern bool extMappingText(const char*, void*&, void*&);
	extern Resource* resourceCreateText(ResourceManager*, const char*);
	subSystems.resourceMgr->addExtMapping(extMappingText);
	subSystems.resourceMgr->addLoader(resourceCreateText, resourceLoadText);

	// Image loaders
	extern bool extMappingBmp(const char*, void*&, void*&);
	extern bool extMappingJpeg(const char*, void*&, void*&);
	extern bool extMappingPng(const char*, void*&, void*&);
	subSystems.resourceMgr->addExtMapping(extMappingBmp);
	subSystems.resourceMgr->addExtMapping(extMappingJpeg);
	subSystems.resourceMgr->addExtMapping(extMappingPng);

	extern Resource* resourceCreateBmp(ResourceManager*, const char*);
	extern Resource* resourceCreateJpeg(ResourceManager*, const char*);
	extern Resource* resourceCreatePng(ResourceManager*, const char*);
	subSystems.resourceMgr->addLoader(resourceCreateBmp, resourceLoadBmp);
	subSystems.resourceMgr->addLoader(resourceCreateJpeg, resourceLoadJpeg);
	subSystems.resourceMgr->addLoader(resourceCreatePng, resourceLoadPng);
}

static void _initFont(SubSystems& subSystems)
{
	subSystems.fontMgr = new FontManager;

	extern Resource* resourceCreateWin32Font(ResourceManager*, const char*, StringHash);
	extern bool resourceLoadWin32Font(ResourceManager*, Resource*);

	Resource* r = resourceCreateWin32Font(subSystems.resourceMgr, "win32Font", stringHash("Font"));
	subSystems.defaultFont = subSystems.resourceMgr->loadAs<Font>(r, resourceLoadWin32Font);
}

SubSystems::SubSystems()
	: userData(NULL)
	, initTaskPool(_initTaskPool), taskPool(NULL)
	, initRenderDriver(_initRenderDriver), renderDriver(NULL), renderContext(NULL)
	, initResourceManager(_initResourceManager), resourceMgr(NULL)
	, initFont(_initFont), fontMgr(NULL)
{
	if(!roSubSystems)
		roSubSystems = this;
}

SubSystems::~SubSystems()
{
	shutdown();

	if(roSubSystems == this)
		roSubSystems = NULL;
}

static MemoryProfiler _memoryProfiler;

Status SubSystems::init()
{
	Status st;
	BsdSocket::initApplication();
	st = _memoryProfiler.init(5000); if(!st) return st;

	initTaskPool(*this);
	initRenderDriver(*this);
	initResourceManager(*this);
	initFont(*this);

	currentCanvas = NULL;

	return Status::ok;
}

void SubSystems::shutdown()
{
	defaultFont = NULL;
	currentCanvas = NULL;

	delete fontMgr;		fontMgr = NULL;
	delete resourceMgr;	resourceMgr = NULL;
	delete taskPool;	taskPool = NULL;

	if(renderDriver) {
		renderDriver->deleteContext(renderContext);
		roDeleteRenderDriver(renderDriver);
	}

	renderDriver = NULL;
	renderContext = NULL;

	_memoryProfiler.shutdown();
	BsdSocket::closeApplication();
}

void SubSystems::tick()
{
	_memoryProfiler.tick();
}

}	// namespace ro

ro::SubSystems* roSubSystems = NULL;
