#ifndef __roBlockAllocator_h__
#define __roBlockAllocator_h__

#include "roBytePtr.h"
#include "roNonCopyable.h"

namespace ro {

// A simple allocator which store pre-allocated big block of memory for fast allocation,
// and must deallocate all allocated memory at once.
struct BlockAllocator : NonCopyable
{
	BlockAllocator(roSize blocksize);
	~BlockAllocator();

	roBytePtr malloc(roSize size);

	// Free ALL allocated blocks
	void free();

// Private
	struct Block
	{
		roSize	size;
		roSize	used;
		roByte*	buffer;
		Block*	next;
	};

	Block*	_head;
	roSize	_blockSize;
};	// BlockAllocator

}	// namespace ro

void roSwap(ro::BlockAllocator& lhs, ro::BlockAllocator& rhs);

#endif	// __roBlockAllocator_h__
