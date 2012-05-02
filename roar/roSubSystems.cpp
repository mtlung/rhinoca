#include "pch.h"
#include "roSubSystems.h"
#include "render/roFont.h"
#include "base/roResource.h"
#include "base/roTaskPool.h"

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
	extern Resource* resourceCreateBmp(const char*, ResourceManager*);
	extern Resource* resourceCreateJpeg(const char*, ResourceManager*);
	extern Resource* resourceCreatePng(const char*, ResourceManager*);
	extern Resource* resourceCreateFont(const char*, ResourceManager*);
	extern bool resourceLoadBmp(Resource*, ResourceManager*);
	extern bool resourceLoadJpeg(Resource*, ResourceManager*);
	extern bool resourceLoadPng(Resource*, ResourceManager*);
	extern bool resourceLoadWinfnt(Resource*, ResourceManager*);

	subSystems.resourceMgr = new ResourceManager;
	subSystems.resourceMgr->taskPool = subSystems.taskPool;
	subSystems.resourceMgr->addFactory(resourceCreateBmp, resourceLoadBmp);
	subSystems.resourceMgr->addFactory(resourceCreateJpeg, resourceLoadJpeg);
	subSystems.resourceMgr->addFactory(resourceCreatePng, resourceLoadPng);
	subSystems.resourceMgr->addFactory(resourceCreateFont, resourceLoadWinfnt);
}

static void _initFont(SubSystems& subSystems)
{
	subSystems.fontMgr = new FontManager;
	subSystems.defaultFont = subSystems.resourceMgr->loadAs<Font>("win.fnt");
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

void SubSystems::init()
{
	initTaskPool(*this);
	initRenderDriver(*this);
	initResourceManager(*this);
	initFont(*this);
}

void SubSystems::shutdown()
{
	defaultFont = NULL;

	delete fontMgr;		fontMgr = NULL;
	delete resourceMgr;	resourceMgr = NULL;
	delete taskPool;	taskPool = NULL;
}

}	// namespace ro

ro::SubSystems* roSubSystems = NULL;
