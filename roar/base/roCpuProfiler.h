#ifndef __roCpuProfiler_h__
#define __roCpuProfiler_h__

#include "roStatus.h"
#include "roStopWatch.h"
#include "roString.h"

namespace ro {

struct CpuProfiler
{
	CpuProfiler();
	~CpuProfiler();

// Operations
	Status init();
	void shutdown();

	void tick();

	void reset();
	float fps() const;
	float timeSinceLastReset() const;

	String report(roSize nameLength=52, float skipMargin=1) const;

// Attributes
	bool enable;

// Private
	struct CallstackNode;

	CallstackNode* _begin(const char name[]);
	void _end(CallstackNode* node);

	void* _rootNode;
	roSize _frameCount;
	StopWatch _stopWatch;
};	// CpuProfiler

struct CpuProfilerScope
{
	CpuProfilerScope(const char name[]);
	~CpuProfilerScope();
	CpuProfiler::CallstackNode* _node;
};

}	// namespace ro

#define roScopeProfile(name) ::ro::CpuProfilerScope roJoinMacro(scopeProfile, __LINE__)(name);

#endif	// __roCpuProfiler_h__
