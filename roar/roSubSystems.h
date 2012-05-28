#include "base/roArray.h"
#include "base/roSharedPtr.h"

struct roRDriver;
struct roRDriverContext;

namespace ro {

struct FontManager;
struct Resource;
struct ResourceManager;
class TaskPool;
typedef SharedPtr<struct Font> FontPtr;

/// Container of most important sub-system
/// This simplify how we access from one sub-system to another sub-system
struct SubSystems
{
	SubSystems();
	~SubSystems();

	typedef void (*CustomInit)(SubSystems& subSystems);

// Operations
	void init();
	void shutdown();

// Attributes
	Array<void*> userData;

	CustomInit initTaskPool;
	TaskPool* taskPool;

	CustomInit initRenderDriver;
	roRDriver* renderDriver;
	roRDriverContext* renderContext;

	CustomInit initResourceManager;
	ResourceManager* resourceMgr;

	CustomInit initFont;
	FontManager* fontMgr;
	FontPtr defaultFont;

// Private
	void* _currentCanvas;	/// I use void* here to emphasis this pointer is for comparison only
};	// SubSystems

// Forward declarations of all build-in loaders
extern bool resourceLoadText(ResourceManager*, Resource*);
extern bool resourceLoadBmp(ResourceManager*, Resource*);
extern bool resourceLoadJpeg(ResourceManager*, Resource*);
extern bool resourceLoadPng(ResourceManager*, Resource*);

}	// namespace ro

extern ro::SubSystems* roSubSystems;
