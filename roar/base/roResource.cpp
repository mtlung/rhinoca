#include "pch.h"
#include "roResource.h"
#include "roCpuProfiler.h"
#include "roLog.h"
#include "roMemory.h"
#include "../math/roMath.h"

namespace ro {

static DefaultAllocator _allocator;

Resource::Resource(const char* p)
	: state(NotLoaded)
	, taskReady(0), taskLoaded(0)
	, createFunc(NULL), loadFunc(NULL)
	, hotness(1)
	, scratch(NULL)
{
	setKey(p);
}

Resource::~Resource()
{
	roAssert(refCount() == 0);
}

ConstString Resource::uri() const
{
	return key();
}

unsigned Resource::refCount() const
{
	return _refCount;
}

void Resource::updateHotness()
{
	if(hotness < roFLT_EPSILON &&
		(state == Resource::Loaded || state == Resource::Aborted))
	{
		unload();
	}
}

ResourceManager::ResourceManager()
	: taskPool(NULL)
{
}

ResourceManager::~ResourceManager()
{
	shutdown();
}

ResourcePtr ResourceManager::load(const char* uri)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	ScopeLock<Mutex> lock(_mutex);

	Resource* r = _resources.find(uri);

	// Find out the factory functions base on the uri
	CreateFunc createFunc = NULL;
	LoadFunc loadFunc = NULL;

	if(!r) {
		for(ExtMappingFunc& i : _extMapping) {
			if(i(uri, (void*&)createFunc, (void*&)loadFunc))
				break;
		}

		if(!createFunc || ! loadFunc) {
			roLog("error", "ResourceManager unable to find extension mapping for uri '%s'\n", uri);
			return NULL;
		}
	}
	else
		loadFunc = r->loadFunc;

	// Create the resource if the uri not found in resource list
	if(!r) {
		r = createFunc(this, uri);
		r->createFunc = createFunc;

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
		r->loadFunc = loadFunc;

		lock.unlockAndCancel();

		if(!loadFunc || !loadFunc(this, r)) {
			r->state = Resource::Aborted;
			return NULL;
		}

		r->hotness = 1000;	// Give a relative hot value right after the resource is loaded
	}

	return r;
}

ResourcePtr ResourceManager::load(Resource* r, LoadFunc loadFunc)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(!r || !loadFunc) return NULL;

	ScopeLock<Mutex> lock(_mutex);

	if(!_resources.insertUnique(*r)) {
		roLog("error", "ResourceManager::load: resource with uri '%s' already exist\n", r->uri().c_str());
		return NULL;
	}

	// We will keep the resource alive such that the time for a Resource destruction
	// is deterministic: always inside ResourceManager::collectUnused()
	sharedPtrAddRef(r);

	// Perform load if not loaded
	if(r->state == Resource::NotLoaded || r->state == Resource::Unloaded) {
		r->state = Resource::Loading;
		r->loadFunc = loadFunc;

		lock.unlockAndCancel();

		if(!loadFunc(this, r)) {
			r->state = Resource::Aborted;
			return NULL;
		}

		r->hotness = 1;	// Give a relative hot value right after the resource is loaded
	}

	return r;
}

Resource* ResourceManager::forget(const char* uri)
{
	roScopeLock(_mutex);

	if(Resource* r = _resources.find(uri)) {
		r->removeThis();
		sharedPtrRelease(r);
		return r;
	}

	return NULL;
}

void ResourceManager::tick()
{
	roScopeLock(_mutex);

	// Every resource will get cooler on every update
	for(Resource* r = _resources.findMin(); r != NULL; r = r->next()) {
		r->hotness *= 0.9f;
	}
}

void ResourceManager::collectInfrequentlyUsed()
{
	roScopeLock(_mutex);

	for(Resource* r = _resources.findMin(); r != NULL; )
	{
		if(r->refCount() == 1 && r->state != Resource::Loading) {
			Resource* next = r->next();
			sharedPtrRelease(r);
			r = next;
			continue;
		}

		r->updateHotness();
		r = r->next();
	}
}

void ResourceManager::abortLoad(Resource* r)
{
	if(!r) return;

	r->state = Resource::Aborted;
	taskPool->resume(r->taskLoaded);
	taskPool->wait(r->taskLoaded);
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
		r->removeThis();
		sharedPtrRelease(r);
		r = next;
	}
}

Resource* ResourceManager::firstResource()
{
	return _resources.findMin();
}

Resource* ResourceManager::nextResource(Resource* current)
{
	if(!current)
		return NULL;
	return current->next();
}

void ResourceManager::addExtMapping(ExtMappingFunc extMappingFunc)
{
	// Push at the font, so late added mapping function always has a higher priority
	_extMapping.insert(0, extMappingFunc);
}

void ResourceManager::addLoader(CreateFunc createFunc, LoadFunc loadFunc)
{
	Factory factory = { createFunc, loadFunc };
	// Push at the font, so late added load function always has a higher priority
	_factories.insert(0, factory);
}

}	// namespace ro
