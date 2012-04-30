#include "base/roSharedPtr.h"

struct roRDriver;
struct roRDriverContext;

namespace ro {

struct FontManager;
struct ResourceManager;
class TaskPool;
typedef SharedPtr<struct Font> FontPtr;

/// Container of most important sub-system
/// This simplify how we access from one sub-system to another sub-system
struct SubSystems
{
	SubSystems();
	~SubSystems();

// Operations
	void init();
	void shutdown();

// Attributes
	FontManager* fontMgr;
	ResourceManager* resourceMgr;
	TaskPool* taskPool;

	roRDriver* renderDriver;
	roRDriverContext* renderContext;

	FontPtr defaultFont;
};	// SubSystems

}	// namespace ro

extern ro::SubSystems* roSubSystems;
