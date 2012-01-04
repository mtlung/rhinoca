#include "pch.h"
#include "roResource.h"
#include "roLog.h"
#include "roMemory.h"

namespace ro {

static DefaultAllocator _allocator;

Resource::Resource(const char* p)
	: state(NotLoaded)
	, taskReady(0), taskLoaded(0)
	, hotness(0)
	, scratch(NULL)
{
	setKey(p);
}

Resource::~Resource()
{}

ConstString Resource::uri() const
{
	return key();
}

unsigned Resource::refCount() const
{
	return _refCount;
}

ResourceManager::ResourceManager()
	: taskPool(NULL)
	, _factories(NULL)
	, _factoryCount(0)
	, _factoryBufCount(0)
{
}

ResourceManager::~ResourceManager()
{
	shutdown();
	_allocator.free(_factories);
}

ResourcePtr ResourceManager::load(const char* uri)
{
	ScopeLock lock(_mutex);

	Resource* r = _resources.find(uri);

	// Create the resource if the uri not found in resource list
	if(!r) {
		for(roSize i=0; !r && i<_factoryCount; ++i)
			r = _factories[i].create(uri, this);

		if(!r) {
			roLog("error", "No loader for \"%s\" can be found\n", uri);
			return NULL;
		}
		roVerify(_resources.insertUnique(*r));

		// We will keep the resource alive such that the time for a Resource destruction
		// is deterministic: always inside ResourceManager::collectUnused()
		sharedPtrAddRef(r);
	}

	// Perform load if not loaded
	if(r->state == Resource::NotLoaded || r->state == Resource::Unloaded) {
		r->state = Resource::Loading;

		lock.unlockAndCancel();

		bool loadInvoked = false;
		for(roSize i=0; i<_factoryCount; ++i) {
			if(_factories[i].load(r, this)) {
				loadInvoked = true;
				r->hotness = 1000;	// Give a relative hot value right after the resource is loaded
				break;
			}
		}
		roAssert(loadInvoked);
	}

	return r;
}

Resource* ResourceManager::forget(const char* uri)
{
	ScopeLock lock(_mutex);

	if(Resource* r = _resources.find(uri)) {
		r->removeThis();
		sharedPtrRelease(r);
		return r;
	}

	return NULL;
}

void ResourceManager::tick()
{
	ScopeLock lock(_mutex);

	// Every resource will get cooler on every update
	for(Resource* r = _resources.findMin(); r != NULL; r = r->next()) {
		r->hotness *= 0.5f;
	}
}

void ResourceManager::collectInfrequentlyUsed()
{
	ScopeLock lock(_mutex);

	for(Resource* r = _resources.findMin(); r != NULL; )
	{
		if(r->refCount() == 1 && r->state != Resource::Loading) {
			Resource* next = r->next();
			sharedPtrRelease(r);
			r = next;
			continue;
		}

		if(r->hotness < 0.00001f &&
			(r->state == Resource::Loaded || r->state == Resource::Aborted))
		{
			r->unload();
		}

		r = r->next();
	}
}

void ResourceManager::abortAllLoader()
{
	// NOTE: Separate into 2 passes can make sure all loading task are set to abort
	for(Resource* r=_resources.findMin(); r; r=r->next()) {
		if(r->state == Resource::Loading)
			r->state = Resource::Aborted;
		// Perform resume for every task in the first pass,
		// prevent blocking task with inter-task dependency
		taskPool->resume(r->taskLoaded);
	}

	for(Resource* r=_resources.findMin(); r; r=r->next())
		taskPool->wait(r->taskLoaded);	// TODO: This still cause problem because of suspended jobs
}

void ResourceManager::shutdown()
{
	abortAllLoader();

	for(Resource* r=_resources.findMin(); r;) {
		Resource* next = r->next();
		sharedPtrRelease(r);
		r = next;
	}
}

void ResourceManager::addFactory(CreateFunc createFunc, LoadFunc loadFunc)
{
	++_factoryCount;

	if(_factoryBufCount < _factoryCount) {
		roSize oldBufCount = _factoryBufCount;
		_factoryBufCount = (_factoryBufCount == 0) ? 2 : _factoryBufCount * 2;
		_factories = _allocator.typedRealloc(_factories, oldBufCount, _factoryBufCount);
	}

	_factories[_factoryCount-1].create = createFunc;
	_factories[_factoryCount-1].load = loadFunc;
}

}	// namespace ro
