#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include "map.h"
#include "../roar/base/roSharedPtr.h"
#include "../roar/base/roString.h"
#include "../roar/base/roTaskPool.h"

class Resource
	// NOTE: No need to use atomic integer as the refCount if no worker thread
	// will hold strong reference to Resource
	: public ro::SharedObject<rhuint>
	, public MapBase<ro::ConstString>::Node<Resource>
	, private Noncopyable
{
protected:
	explicit Resource(const char* uri);

	virtual ~Resource();

public:
// Operations
	/// Unload the data which occupy most memory, and only bare information can remain.
	/// For example, after a Texture is unloaded, the pixel data are gone but we still keep the width/height
	virtual void unload() {}

// Attributes
	ro::ConstString uri() const;

	enum State { NotLoaded, Loading, Ready, Loaded, Unloaded, Aborted };
	State state;	///!< Important: changing of this value must be performed on main thread

	ro::TaskId taskReady, taskLoaded;

	float hotness;	///!< For tracking resource usage and perform unload when resource is scarce

	void* scratch;	///! Hold any temporary needed during loading

	unsigned refCount() const;
};	// Resource

typedef ro::SharedPtr<Resource> ResourcePtr;

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

// Operations
	/// This function is async, you need to wait for the resource's TaskId to do synchronization
	/// @note: Recursive and re-entrant
	ResourcePtr load(const char* uri);

	template<class T>
	ro::SharedPtr<T> loadAs(const char* uri) { return dynamic_cast<T*>(load(uri).get()); }

	/// Remove the resource from the management of the ResourceManager
	Resource* forget(const char* uri);

	// Call this on every frame
	void update();

	/// Check for infrequently used resource and unload them
	void collectInfrequentlyUsed();

	/// Abort all loading operation, allow a faster shut down of the engine
	void abortAllLoader();

// Factories
	typedef Resource* (*CreateFunc)(const char* uri, ResourceManager* mgr);
	typedef bool (*LoadFunc)(Resource* resource, ResourceManager* mgr);
	void addFactory(CreateFunc createFunc, LoadFunc loadFunc);

// Attributes
	Rhinoca* rhinoca;
	ro::TaskPool* taskPool;

protected:
	ro::Mutex _mutex;

	struct Tuple { CreateFunc create; LoadFunc load; };
	Tuple* _factories;
	int _factoryCount;
	int _factoryBufCount;

	typedef Map<Resource> Resources;
	Resources _resources;
};	// ResourceManager

extern bool uriExtensionMatch(const char* uri, const char* extension);

#endif	// __RESOURCE_H__
