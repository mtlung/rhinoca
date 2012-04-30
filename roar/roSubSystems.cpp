#include "pch.h"
#include "roSubSystems.h"
#include "render/roFont.h"
#include "base/roResource.h"
#include "base/roTaskPool.h"

namespace ro {

SubSystems::SubSystems()
	: fontMgr(NULL)
	, resourceMgr(NULL)
	, taskPool(NULL)
	, renderDriver(NULL)
	, renderContext(NULL)
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

extern Resource* resourceCreateBmp(const char*, ResourceManager*);
extern Resource* resourceCreateJpeg(const char*, ResourceManager*);
extern Resource* resourceCreatePng(const char*, ResourceManager*);
extern Resource* resourceCreateFont(const char*, ResourceManager*);
extern bool resourceLoadBmp(Resource*, ResourceManager*);
extern bool resourceLoadJpeg(Resource*, ResourceManager*);
extern bool resourceLoadPng(Resource*, ResourceManager*);
extern bool resourceLoadWinfnt(Resource*, ResourceManager*);

void SubSystems::init()
{
	taskPool = new TaskPool;
	taskPool->init(2);

	resourceMgr = new ResourceManager;
	resourceMgr->taskPool = taskPool;

	fontMgr = new FontManager;

	resourceMgr->addFactory(resourceCreateBmp, resourceLoadBmp);
	resourceMgr->addFactory(resourceCreateJpeg, resourceLoadJpeg);
	resourceMgr->addFactory(resourceCreatePng, resourceLoadPng);
	resourceMgr->addFactory(resourceCreateFont, resourceLoadWinfnt);

	defaultFont = resourceMgr->loadAs<Font>("win.fnt");
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
