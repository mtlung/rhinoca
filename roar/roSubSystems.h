#ifndef __roSubSystems_h__
#define __roSubSystems_h__

#include "base/roArray.h"
#include "base/roSharedPtr.h"

struct roAudioDriver;
struct roInputDriver;
struct roRDriver;
struct roRDriverContext;

namespace ro {

struct FontManager;
struct Resource;
struct ResourceManager;
class TaskPool;
typedef SharedPtr<struct Resource> ResourcePtr;
typedef SharedPtr<struct Font> FontPtr;

/// Container of most important sub-system
/// This simplify how we access from one sub-system to another sub-system
struct SubSystems
{
	SubSystems();
	~SubSystems();

	typedef void (*CustomInit)(SubSystems& subSystems);

// Operations
	Status	init			();
	void	shutdown		();
	void	tick			();
	void	processEvents	(void** platformSPecificData, roSize numData);

// Attributes
	Array<void*> userData;

	CustomInit initTaskPool;
	TaskPool* taskPool;

	// Audio driver
	CustomInit initAudioDriver;
	roAudioDriver* audioDriver;

	// Input driver
	roInputDriver* inputDriver;

	// Render driver
	CustomInit initRenderDriver;
	roRDriver* renderDriver;
	roRDriverContext* renderContext;

	// Resource management
	CustomInit initResourceManager;
	ResourceManager* resourceMgr;
	Array<ResourcePtr> systemResource;	/// A list of resource that should always keep in the system (eg. Default Font)

	// Font
	CustomInit initFont;
	FontManager* fontMgr;
	FontPtr defaultFont;

	/// You may need set it to NULL when your viewport resize, to force the current canvas to be refreshed
	/// I use void* here to emphasis this pointer is for comparison only
	void* currentCanvas;

	void enableCpuProfiler(bool b);

// Frame statistics
	roSize frameNumber;
	float maxFrameDuration;
	float averageFrameDuration;
};	// SubSystems

// Forward declarations of all build-in loaders
extern bool resourceLoadText(ResourceManager*, Resource*);
extern bool resourceLoadImage(ResourceManager*, Resource*);
extern bool resourceLoadBmp(ResourceManager*, Resource*);
extern bool resourceLoadJpeg(ResourceManager*, Resource*);
extern bool resourceLoadPng(ResourceManager*, Resource*);

}	// namespace ro

extern ro::SubSystems* roSubSystems;

#endif	// __roSubSystems_h__
