#include "pch.h"
#include "resource.h"
#include "common.h"
#include "../roar/base/roLog.h"
#include <string.h>

using namespace ro;

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

ro::ConstString Resource::uri() const
{
	return getKey();
}

unsigned Resource::refCount() const
{
	return _refCount;
}

ResourceManager::ResourceManager()
	: rhinoca(NULL)
	, taskPool(NULL)
	, _factories(NULL)
	, _factoryCount(0)
	, _factoryBufCount(0)
{
}

ResourceManager::~ResourceManager()
{
	abortAllLoader();

	for(Resource* r=_resources.findMin(); r;) {
		Resource* next = r->next();
		intrusivePtrRelease(r);
		r = next;
	}

	rhdelete(_factories);
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
		taskPool->wait(r->taskLoaded);
}

ResourcePtr ResourceManager::load(const char* uri)
{
	ScopeLock lock(_mutex);

	Resource* r = _resources.find(uri);

	// Create the resource if the uri not found in resource list
	if(!r) {
		for(int i=0; !r && i<_factoryCount; ++i)
			r = _factories[i].create(uri, this);

		if(!r) {
			roLog("error", "No loader for \"%s\" can be found\n", uri);
			return NULL;
		}
		RHVERIFY(_resources.insertUnique(*r));

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
		RHASSERT(loadInvoked);
	}

	return r;
}

Resource* ResourceManager::forget(const char* uri)
{
	ScopeLock lock(_mutex);

	if(Resource* r = _resources.find(uri)) {
		r->removeThis();
		intrusivePtrRelease(r);
		return r;
	}

	return NULL;
}

void ResourceManager::update()
{
	ScopeLock lock(_mutex);

	// Every resource will get cooler on every update
	for(Resource* r = _resources.findMin(); r != NULL; r = r->next()) {
		// TODO: Will this cause denormailization penalty?
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
			intrusivePtrRelease(r);
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

bool uriExtensionMatch(const char* uri, const char* extension)
{
	rhuint uriLen = strlen(uri);
	rhuint extLen = strlen(extension);

	if(uriLen < extLen) return false;
	return roStrCaseCmp(uri + uriLen - extLen, extension) == 0;
}
