#include "pch.h"
#include "roBlockAllocator.h"
#include "roMemory.h"
#include "roUtility.h"

namespace ro {

BlockAllocator::BlockAllocator(roSize blocksize)
	: _head(NULL)
	, _blockSize(blocksize)
{}

BlockAllocator::~BlockAllocator()
{
	while(_head) {
		Block* block = _head->next;
		roFree(_head);
		_head = block;
	}
}

roBytePtr BlockAllocator::malloc(roSize size)
{
	if((_head && _head->used + size > _head->size) || !_head)
	{
		// Calculate needed size for allocation
		roSize allocSize = roMaxOf2(size, _blockSize) + sizeof(Block);

		// Create new block
		roBytePtr p = roMalloc(allocSize);
		if(!p) return NULL;

		Block* b = p.cast<Block>();
		b->size = allocSize;
		b->used = sizeof(Block);
		b->buffer = p;
		b->next = _head;
		_head = b;
	}

	void* ptr = _head->buffer + _head->used;
	_head->used += size;
	return ptr;
}

void BlockAllocator::free()
{	
	BlockAllocator tmp(0);
	roSwap(tmp, *this);
}

}	// namespace ro

void roSwap(ro::BlockAllocator& lhs, ro::BlockAllocator& rhs)
{
	roSwap(lhs._blockSize, rhs._blockSize);
	roSwap(lhs._head, rhs._head);
}
