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
protected:
	explicit Resource(const char* path);

	virtual ~Resource();

public:
// Operations
	/// Unload the data which occupy most memory, and only bare information can remain.
	/// For example, after a Texture is unloaded, the pixel data are gone but we still keep the width/height
	virtual void unload() {}

// Attributes
	enum State { NotLoaded, Loading, Ready, Loaded, Unloaded, Aborted };
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

	template<class T>
	IntrusivePtr<T> loadAs(const char* path) { return dynamic_cast<T*>(load(path).get()); }

	// Call this on every frame
	void update();

	void collectUnused();

// Factories
	typedef Resource* (*CreateFunc)(const char* path, ResourceManager* mgr);
	typedef bool (*LoadFunc)(Resource* resource, ResourceManager* mgr);
	void addFactory(CreateFunc createFunc, LoadFunc loadFunc);

// Attributes
	TaskPool* taskPool;

protected:
	Mutex _mutex;

	struct Tuple { CreateFunc create; LoadFunc load; };
	Tuple* _factories;
	int _factoryCount;
	int _factoryBufCount;

	typedef Map<Resource> Resources;
	Resources _resources;
};	// ResourceManager

#endif	// __RESOURCE_H__
