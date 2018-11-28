#ifndef __roMemoryProfiler_h__
#define __roMemoryProfiler_h__

#include "../network/roSocket.h"

namespace ro {

struct MemoryProfiler
{
	MemoryProfiler();
	~MemoryProfiler();

	/// The memory profiler must have an external program to analysis the data
	Status init(roUint16 listeningPort);
	static void flush();
	void shutdown();

// Private
	BsdSocket _listeningSocket;
	BsdSocket _acceptorSocket;
	roSize _frameCount;
};	// MemoryProfiler

}	// namespace ro

#endif	// __roMemoryProfiler_h__
