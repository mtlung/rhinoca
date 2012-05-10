#ifndef __roResource_h__
#define __roResource_h__

#include "roArray.h"
#include "roAtomic.h"
#include "roMap.h"
#include "roSharedPtr.h"
#include "roString.h"
#include "roTaskPool.h"

namespace ro {

struct ResourceManager;

struct Resource
	: public SharedObject<AtomicInteger>
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

	virtual ConstString resourceType() const { return ""; }

	enum State { NotLoaded, Loading, Ready, Loaded, Unloaded, Aborted };
	State state;	///!< Important: changing of this value must be performed on main thread

	TaskId taskReady, taskLoaded;

	Resource* (*createFunc)(ResourceManager* mgr, const char* uri);
	bool (*loadFunc)(ResourceManager*, Resource*);

	float hotness;	///!< For tracking resource usage and perform unload when resource is scarce

	void* scratch;	///! Hold any temporary needed during loading

	unsigned refCount() const;
};	// Resource

typedef SharedPtr<Resource> ResourcePtr;

struct ResourceManager
{
	ResourceManager();
	~ResourceManager();

// Loader function
	typedef Resource* (*CreateFunc)(ResourceManager* mgr, const char* uri);
	typedef bool (*LoadFunc)(ResourceManager* mgr, Resource* resource);
	void addLoader(CreateFunc createFunc, LoadFunc loadFunc);

// Extension mapping
	typedef bool (*ExtMappingFunc)(const char* uri, void*& createFunc, void*& loadFunc);
	void addExtMapping(ExtMappingFunc extMappingFunc);

// Operations
	/// Load functions are async, you need to wait for the resource's TaskId to do synchronization
	/// @note: Recursive and re-entrant
	ResourcePtr load(const char* uri);
	ResourcePtr load(Resource* r, LoadFunc loadFunc);

	template<class T>
	SharedPtr<T> loadAs(const char* uri) { return dynamic_cast<T*>(load(uri).get()); }

	template<class T>
	SharedPtr<T> loadAs(Resource* r, LoadFunc loadFunc) { return dynamic_cast<T*>(load(r, loadFunc).get()); }

	/// Remove the resource from the management of the ResourceManager
	Resource* forget(const char* uri);

	/// Call this on every frame
	void tick();

	/// Check for infrequently used resource and unload them
	void collectInfrequentlyUsed();

	/// Abort all loading operation, allow a faster shut down of the engine
	void abortAllLoader();

	void shutdown();

// Attributes
	TaskPool* taskPool;

// Private
	Mutex _mutex;

	Array<ExtMappingFunc> _extMapping;

	struct Factory { CreateFunc create; LoadFunc load; };
	Array<Factory> _factories;

	typedef Map<Resource> Resources;
	Resources _resources;
};	// ResourceManager

}	// namespace ro

#endif	// __roResource_h__
