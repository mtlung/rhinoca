#ifndef __roResource_h__
#define __roResource_h__

#include "roMap.h"
#include "roSharedPtr.h"
#include "roString.h"
#include "roTaskPool.h"

namespace ro {

struct Resource
	// NOTE: No need to use atomic integer as the refCount if no worker thread
	// will hold strong reference to Resource
	: public SharedObject<unsigned>
	, public MapNode<ConstString, Resource>
	, private NonCopyable
{
	explicit Resource(const char* uri);

	virtual ~Resource();

// Operations
	/// Unload the data which occupy most memory, and only bare information can remain.
	/// For example, after a Texture is unloaded, the pixel data are gone but we still keep the width/height
	virtual void unload() {}

// Attributes
	ConstString uri() const;

	enum State { NotLoaded, Loading, Ready, Loaded, Unloaded, Aborted };
	State state;	///!< Important: changing of this value must be performed on main thread

	TaskId taskReady, taskLoaded;

	float hotness;	///!< For tracking resource usage and perform unload when resource is scarce

	void* scratch;	///! Hold any temporary needed during loading

	unsigned refCount() const;
};	// Resource

typedef SharedPtr<Resource> ResourcePtr;

struct ResourceManager
{
	ResourceManager();
	~ResourceManager();

// Operations
	/// This function is async, you need to wait for the resource's TaskId to do synchronization
	/// @note: Recursive and re-entrant
	ResourcePtr load(const char* uri);

	template<class T>
	SharedPtr<T> loadAs(const char* uri) { return dynamic_cast<T*>(load(uri).get()); }

	/// Remove the resource from the management of the ResourceManager
	Resource* forget(const char* uri);

	/// Call this on every frame
	void tick();

	/// Check for infrequently used resource and unload them
	void collectInfrequentlyUsed();

	/// Abort all loading operation, allow a faster shut down of the engine
	void abortAllLoader();

// Factories
	typedef Resource* (*CreateFunc)(const char* uri, ResourceManager* mgr);
	typedef bool (*LoadFunc)(Resource* resource, ResourceManager* mgr);
	void addFactory(CreateFunc createFunc, LoadFunc loadFunc);

// Attributes
	TaskPool* taskPool;

// Private
	Mutex _mutex;

	struct Tuple { CreateFunc create; LoadFunc load; };
	Tuple* _factories;
	roSize _factoryCount;
	roSize _factoryBufCount;

	typedef Map<Resource> Resources;
	Resources _resources;
};	// ResourceManager

}	// namespace ro

#endif	// __roResource_h__
