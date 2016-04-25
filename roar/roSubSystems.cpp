#include "pch.h"
#include "roSubSystems.h"
#include "render/roFont.h"
#include "base/roCpuProfiler.h"
#include "base/roLog.h"
#include "base/roMemoryProfiler.h"
#include "base/roResource.h"
#include "base/roReflection.h"
#include "base/roTaskPool.h"
#include "math/roMath.h"
#include "audio/roAudioDriver.h"
#include "input/roInputDriver.h"
#include "network/roSocket.h"
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
#if roOS_WIN
	extern bool extMappingImage(const char*, void*&, void*&);
	subSystems.resourceMgr->addExtMapping(extMappingImage);
#else
	extern bool extMappingBmp(const char*, void*&, void*&);
	extern bool extMappingJpeg(const char*, void*&, void*&);
	extern bool extMappingPng(const char*, void*&, void*&);
#endif

	extern Resource* resourceCreateImage(ResourceManager*, const char*);
	extern Resource* resourceCreateBmp(ResourceManager*, const char*);
	extern Resource* resourceCreateJpeg(ResourceManager*, const char*);
	extern Resource* resourceCreatePng(ResourceManager*, const char*);
#if roOS_WIN
	subSystems.resourceMgr->addLoader(resourceCreateBmp, resourceLoadImage);
#else
	subSystems.resourceMgr->addLoader(resourceCreateBmp, resourceLoadBmp);
	subSystems.resourceMgr->addLoader(resourceCreateJpeg, resourceLoadJpeg);
	subSystems.resourceMgr->addLoader(resourceCreatePng, resourceLoadPng);
#endif
}

static void _initFont(SubSystems& subSystems)
{
	subSystems.fontMgr = new FontManager;

	extern Resource* resourceCreateWin32Font(ResourceManager*, const char*, StringHash);
	extern bool resourceLoadWin32Font(ResourceManager*, Resource*);

	Resource* r = resourceCreateWin32Font(subSystems.resourceMgr, "win32Font", stringHash("Font"));
	subSystems.defaultFont = subSystems.resourceMgr->loadAs<Font>(r, resourceLoadWin32Font);
	subSystems.systemResource.pushBack(subSystems.defaultFont);
}

SubSystems::SubSystems()
	: userData(NULL)
	, initTaskPool(_initTaskPool), taskPool(NULL)
	, audioDriver(NULL)
	, inputDriver(NULL)
	, initRenderDriver(_initRenderDriver), renderDriver(NULL), renderContext(NULL)
	, initResourceManager(_initResourceManager), resourceMgr(NULL)
	, initFont(_initFont), fontMgr(NULL)
	, frameNumber(0)
	, maxFrameDuration(0)
	, averageFrameDuration(0)
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

static CpuProfiler _cpuProfiler;
static MemoryProfiler _memoryProfiler;

Status SubSystems::init()
{
	shutdown();

	if(!_initCounter.tryInit())
		return roStatus::ok;

	Status st;
	BsdSocket::initApplication();
	st = _cpuProfiler.init(); if(!st) return st;
	st = _memoryProfiler.init(0); if(!st) return st;

	_cpuProfiler.enable = false;

	st = registerReflection();
	if(!st) return st;

	initTaskPool(*this);
	initRenderDriver(*this);
	initResourceManager(*this);
	initFont(*this);

	currentCanvas = NULL;

	audioDriver = roNewAudioDriver();
	roInitAudioDriver(audioDriver, "");

	inputDriver = roNewInputDriver();
	roInitInputDriver(inputDriver, "");

	frameNumber = 0;
	maxFrameDuration = 0;
	averageFrameDuration = 0;

	return Status::ok;
}

void SubSystems::shutdown()
{
	if(!_initCounter.tryShutdown())
		return;

	defaultFont = NULL;
	currentCanvas = NULL;

	// Early out all system resource loading process
	if(resourceMgr)
		resourceMgr->abortAllLoader();
	systemResource.clear();

	delete fontMgr;		fontMgr = NULL;
	delete resourceMgr;	resourceMgr = NULL;
	delete taskPool;	taskPool = NULL;

	if(renderDriver) {
		renderDriver->deleteContext(renderContext);
		roDeleteRenderDriver(renderDriver);
	}

	renderDriver = NULL;
	renderContext = NULL;

	roDeleteAudioDriver(audioDriver);
	audioDriver = NULL;

	roDeleteInputDriver(inputDriver);
	inputDriver = NULL;

	_cpuProfiler.shutdown();
	_memoryProfiler.shutdown();
	BsdSocket::closeApplication();

	Reflection::reflection.reset();
}

void SubSystems::tick()
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(taskPool)
		taskPool->doSomeTask(1.0f / 100.0f);

	if(resourceMgr) {
		resourceMgr->tick();
		resourceMgr->collectInfrequentlyUsed();
	}

	if(inputDriver)
		inputDriver->tick(inputDriver);

	if(audioDriver)
		audioDriver->tick(audioDriver);

	if(renderContext) {
		++frameNumber;
		averageFrameDuration = roStepRunAvg(averageFrameDuration, renderContext->lastFrameDuration, 60);
		maxFrameDuration = roMaxOf2(maxFrameDuration, renderContext->lastFrameDuration);
	}

	if(_cpuProfiler.enable) {
		_cpuProfiler.tick();
		if(_cpuProfiler.timeSinceLastReset() >= 3) {
			String s = _cpuProfiler.report();
			roLog("verbose", "%s\n", s.c_str());
			_cpuProfiler.reset();
		}
	}
}

void SubSystems::enableCpuProfiler(bool b)
{
	_cpuProfiler.enable = b;
}

}	// namespace ro

ro::SubSystems* roSubSystems = NULL;
