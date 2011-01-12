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
{
}

ResourceManager::~ResourceManager()
{
}

ResourcePtr ResourceManager::load(const char* path)
{
	ScopeLock lock(_mutex);

	Resource* r = _resources.find(path);

	if(!r) {
		r = ResourceFactory::singleton().create(path, this);
		if(r) _resources.insertUnique(*r);
	}

	return r;
}

void ResourceManager::update()
{
}

void ResourceManager::collectUnused()
{
}

ResourceFactory::ResourceFactory()
	: _factories(NULL)
	, _factoryCount(0)
	, _factoryBufCount(0)
{
}

ResourceFactory::~ResourceFactory()
{
	rhdelete(_factories);
}

ResourceFactory& ResourceFactory::singleton()
{
	static ResourceFactory factory;
	return factory;
}

Resource* ResourceFactory::create(const char* path, ResourceManager* mgr)
{
	for(int i=0; i<_factoryCount; ++i) {
		if(Resource* r = _factories[i](path, mgr))
			return r;
	}
	return NULL;
}

void ResourceFactory::addFactory(FactoryFunc factory)
{
	++_factoryCount;

	if(_factoryBufCount < _factoryCount) {
		int oldBufCount = _factoryBufCount;
		_factoryBufCount = (_factoryBufCount == 0) ? 2 : _factoryBufCount * 2;
		_factories = rhrenew(_factories, oldBufCount, _factoryBufCount);
	}

	_factories[_factoryCount-1] = factory;
}
