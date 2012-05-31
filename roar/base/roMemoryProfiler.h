#ifndef __roMemoryProfiler_h__
#define __roMemoryProfiler_h__

#include "roSocket.h"
#include "roStatus.h"

namespace ro {

struct MemoryProfiler
{
	MemoryProfiler();
	~MemoryProfiler();

	/// The memory profiler must have an external program to analysis the data
	Status init(roUint16 listeningPort);

	void shutdown();

	void tick();

// Private
	BsdSocket _listeningSocket;
	BsdSocket _acceptorSocket;
	roSize _frameCount;
};	// MemoryProfiler

}	// namespace ro

#endif	// __roMemoryProfiler_h__
