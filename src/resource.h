#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include "intrusiveptr.h"
#include "map.h"
#include "rhstring.h"
#include "taskpool.h"

class Resource
	// NOTE: No need to use atomic interger as the refCount if no worker thread
	// will hold strong reference to Resource
	: public IntrusiveSharedObject<rhuint>
	, public MapBase<FixString>::Node<Resource>
	, private Noncopyable
{
public:
	explicit Resource(const char* path);
	virtual ~Resource();

// Operations
	/// Unload the data which occupy most memory, and only bare information can remain.
	/// For example, after a Texture is unloaded, the pixel data are gone but we still keep the width/height
	virtual void unload() {}

// Attributes
	enum State { NotLoaded, Ready, Loaded, Unloaded, Aborted };
	State state;
	TaskId taskReady, taskLoaded;
	float hotness;	///!< For tracking resource usage and perform unload when resource is scarce
	const FixString path;

	void* scratch;	///! Hold any temporary needed during loading
};	// Resource

typedef IntrusivePtr<Resource> ResourcePtr;

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

// Operations
	/// @note: Recursive and re-entrant
	ResourcePtr load(const char* path);

	void update();

	void collectUnused();

// Attributes
	TaskPool* taskPool;

protected:
	Mutex _mutex;

	typedef Map<Resource> Resources;
	Resources _resources;
};	// ResourceManager

class ResourceFactory
{
public:
	ResourceFactory();
	~ResourceFactory();

	static ResourceFactory& singleton();

	/// Returns a new Resource if the path/file extension matched, null otherwise
	Resource* create(const char* path, ResourceManager* mgr);

	/// Loads the Resource if the path/file extension matched, do nothing otherwise
	void load(Resource* resource, ResourceManager* mgr);

	/// A function that return a new Resource if the path/file extension matched, otherwise return null
	typedef Resource* (*FactoryFunc)(const char* path, ResourceManager* mgr);
	void addFactory(FactoryFunc factory);

protected:
	FactoryFunc* _factories;
	int _factoryCount;
	int _factoryBufCount;
};	// ResourceFactory

#endif	// __RESOURCE_H__
