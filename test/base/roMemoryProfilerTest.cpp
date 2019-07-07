#include "pch.h"
#include "../../roar/base/roMemoryProfiler.h"
#include "../../roar/platform/roPlatformHeaders.h"

TEST(MemoryProfilerTest)
{
	using namespace ro;

	{	void* p = roMalloc(0);
		p = roRealloc(p, 0, 100);
		p = roRealloc(p, 100, 1024);
		p = roRealloc(p, 1024, 1);
		roFree(p);
		MemoryProfiler::flush();
	}

	{	// Basic virtual memory functions
		void* p = ::VirtualAlloc(NULL, 0, MEM_RESERVE, PAGE_READWRITE);
		CHECK(p == NULL);
		p = ::VirtualAlloc(NULL, 1024, MEM_RESERVE, PAGE_READWRITE);
		CHECK(p);
		CHECK(!::VirtualAlloc((char*)p + 1, 1024, MEM_COMMIT, PAGE_READWRITE));		// Must give the base address for MEM_COMMIT, althought it fails but memory is commited through NtAllocateVirtualMemory
		p = ::VirtualAlloc(p, 1024, MEM_COMMIT, PAGE_READWRITE);					// mem + 4096
		CHECK(p);

		CHECK(::VirtualFree(p, 1024, MEM_DECOMMIT));								// mem - 4096
		CHECK(!::VirtualFree(p, 1024, MEM_RELEASE));								// size must be 0 for MEM_RELEASE
		CHECK(::VirtualFree(p, 0, MEM_RELEASE));
		MemoryProfiler::flush();
	}

	{	// Combined flags
		void* p = ::VirtualAlloc(NULL, 0, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		CHECK(p == NULL);
		p = ::VirtualAlloc(NULL, 4097, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);	// mem + 8192
		CHECK(p);

		CHECK(!::VirtualFree(p, 0, MEM_DECOMMIT | MEM_RELEASE));					// Cannot mix MEM_DECOMMIT and MEM_RELEASE
		CHECK(::VirtualFree(p, 0, MEM_RELEASE));									// mem - 8192
		MemoryProfiler::flush();
	}

	{	// Partial commit
		void* b = ::VirtualAlloc(NULL, 4096 * 10, MEM_RESERVE, PAGE_READWRITE);
		void* p = NULL;

		p = ::VirtualAlloc((char*)b +  4096, 4096, MEM_COMMIT, PAGE_READWRITE);		// mem + 4096
		p = ::VirtualAlloc((char*)b +  4096, 4096, MEM_COMMIT, PAGE_READWRITE);		// mem + 0, Commit on already committed region
		p = ::VirtualAlloc((char*)b + 12288, 8192, MEM_COMMIT, PAGE_READWRITE);		// mem + 8192
		p = ::VirtualAlloc((char*)b +     0,40960, MEM_COMMIT, PAGE_READWRITE);		// mem + 28672, some regions already committed, and creating 2 new entries one at front and one at back

		CHECK(::VirtualFree((char*)b + 4096, 4096, MEM_DECOMMIT));					// mem - 4096
		CHECK(::VirtualFree((char*)b + 4096, 4096, MEM_DECOMMIT));					// mem - 0, Decommit on already decommitted region
		CHECK(::VirtualFree((char*)b +12288, 8192, MEM_DECOMMIT));					// mem - 8192
		CHECK(::VirtualFree((char*)b +    0,36864, MEM_DECOMMIT));					// mem - 24576, some regions already decommitted

		p = ::VirtualAlloc((char*)b +  4096, 4096, MEM_COMMIT, PAGE_READWRITE);		// Do all the above allocation once more
		p = ::VirtualAlloc((char*)b +  4096, 4096, MEM_COMMIT, PAGE_READWRITE);
		p = ::VirtualAlloc((char*)b + 12288, 8192, MEM_COMMIT, PAGE_READWRITE);
		p = ::VirtualAlloc((char*)b +     0,40960, MEM_COMMIT, PAGE_READWRITE);

		CHECK(::VirtualFree((char*)b +    0,    0, MEM_DECOMMIT));					// Decommit all at once

		p = ::VirtualAlloc((char*)b +  4096, 4096, MEM_COMMIT, PAGE_READWRITE);

		CHECK(::VirtualFree(b, 0, MEM_RELEASE));									// mem - 4096
		MemoryProfiler::flush();
	}
}
