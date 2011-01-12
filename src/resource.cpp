#include "pch.h"
#include "resource.h"

Resource::Resource(const char* p)
	: path(p)
	, state(NotLoaded)
	, taskReady(0), taskLoaded(0)
	, hotness(0)
	, scratch(NULL)
{}

Resource::~Resource()
{}

ResourceManager::ResourceManager()
	: taskPool(NULL)
	, _factories(NULL)
	, _factoryCount(0)
	, _factoryBufCount(0)
{
}

ResourceManager::~ResourceManager()
{
	Resource* r = NULL;
	while(r = _resources.findMin())
		intrusivePtrRelease(r);

	rhdelete(_factories);
}

ResourcePtr ResourceManager::load(const char* path)
{
	ScopeLock lock(_mutex);

	Resource* r = _resources.find(path);

	// Create the resource if the path not found in resource list
	if(!r) {
		for(int i=0; !r && i<_factoryCount; ++i)
			r = _factories[i].create(path, this);

		if(!r) return NULL;
		VERIFY(_resources.insertUnique(*r));

		// We will keep the resource alive such that the time for a Resource destruction
		// is deterministic: always inside ResourceManager::collectUnused()
		intrusivePtrAddRef(r);
	}

	// Perform load if not loaded
	if(r->state == Resource::NotLoaded || r->state == Resource::Unloaded) {
		r->state = Resource::Loading;

		lock.unlockAndCancel();

		bool loadInvoked = false;
		for(int i=0; i<_factoryCount; ++i) {
			if(_factories[i].load(r, this)) {
				loadInvoked = true;
				r->hotness = 1000;	// Give a relative hot value right after the resource is loaded
				break;
			}
		}
		ASSERT(loadInvoked);
	}

	return r;
}

void ResourceManager::update()
{
	ScopeLock lock(_mutex);

	// Every resource will get cooler on every update
	for(Resource* r = _resources.findMin(); r != NULL; r = r->next()) {
		r->hotness *= 0.5f;
	}
}

void ResourceManager::collectUnused()
{
	ScopeLock lock(_mutex);

	for(Resource* r = _resources.findMin(); r != NULL; r = r->next()) {
		if(r->hotness == 0)
			r->unload();
	}
}

void ResourceManager::addFactory(CreateFunc createFunc, LoadFunc loadFunc)
{
	++_factoryCount;

	if(_factoryBufCount < _factoryCount) {
		int oldBufCount = _factoryBufCount;
		_factoryBufCount = (_factoryBufCount == 0) ? 2 : _factoryBufCount * 2;
		_factories = rhrenew(_factories, oldBufCount, _factoryBufCount);
	}

	_factories[_factoryCount-1].create = createFunc;
	_factories[_factoryCount-1].load = loadFunc;
}
