#ifndef __roObjectTable_h__
#define __roObjectTable_h__

#include "roArray.h"
#include "roTypeCast.h"

namespace ro {

/// Id to object look up table
/// http://bitsquid.blogspot.com/2011/09/managing-decoupling-part-4-id-lookup.html
template<class T>
struct ObjectTable
{
	ObjectTable()
	{
		_freeListEnque = _nullIdx;
		_freeListDeque = _nullIdx;
	}

// Operations:
	roUint32 create()
	{
		roAssert(_index.size() < TypeOf<roUint16>::valueMax());

		// Not enough entry in the index list, enlarge it
		if(_freeListDeque >= _index.size()) {
			_freeListDeque = num_cast<roUint16>(_index.size());
			_Index idx = { _index.size(), _nullIdx, _nullIdx };
			arrayIncSize(_index, idx);
		}

		// We are taking away the last entry in the free list, so invalidate _freeListEnque
		if(_freeListEnque == _freeListDeque)
			_freeListEnque = _nullIdx;

		// Setup entry in the index array
		_Index& i = _index[_freeListDeque];
		i.id += _countInc;	// Overflow here is rarely a problem

		i.objectIdx = num_cast<roUint16>(_objects.size());

		_freeListDeque = i.next;
		i.next = _nullIdx;

		// Setup entry in the object array
		arrayIncSize(_objects, _Object());	// NOTE: Unnecessary copying
		_objects.back().id = i.id;

		return i.id;
	}

	T* lookup(roUint32 id)
	{
		const roUint32 indexIdx = id & _indexMask;
		if(indexIdx >= _index.size()) return NULL;

		const _Index& i = _index[indexIdx];
		if((i.id & _countMask) != (id & _countMask)) return NULL;
		if(i.objectIdx >= _objects.size()) return NULL;

		return &_objects[i.objectIdx]._object;
	}

	void destroy(roUint32 id)
	{
		const roUint16 indexIdx = id & _indexMask;
		if(indexIdx >= _index.size()) return;

		_Index& i = _index[indexIdx];
		if((i.id & _countMask) != (id & _countMask)) return;
		if(i.objectIdx >= _objects.size()) return;

		// Removing the entry by swapping trick
		_index[_objects.back().id & _indexMask].objectIdx = i.objectIdx;
		arrayRemoveBySwap(_objects, i.objectIdx);

		// For safety, invalidate the object index in the index list
		i.objectIdx = _nullIdx;

		// Adjust the free list
		if(_freeListDeque == _nullIdx)
			_freeListDeque = indexIdx;
		else
			_index[_freeListEnque].next = indexIdx;

		_freeListEnque = indexIdx;
	}

	// For iterating all the objects, the order is non-deterministic
	T&			iterateAt(roSize idx)		{ return _objects[idx]._object; }
	const T&	iterateAt(roSize idx) const	{ return _objects[idx]._object; }

// Attributes:
	roUint16 size() const { return num_cast<roUint16>(_objects.size()); }

// Private:
	struct _Index {
		roUint32 id;
		roUint16 objectIdx;
		roUint16 next;
	};

	struct _Object {
		roUint32 id;
		// TODO: Add padding according to the alignment requirement of T
		T _object;
	};

	static const roUint32 _indexMask = 0x0000FFFF;
	static const roUint32 _countMask = 0xFFFF0000;
	static const roUint32 _countInc = 0x00010000;
	static const roUint16 _nullIdx = roUint16(-1);

	roUint16 _freeListEnque;	// We enqueue and deque at different point,
	roUint16 _freeListDeque;	// so that less chance for id collision when overflow
	Array<_Index> _index;
	Array<_Object> _objects;
};	// ObjectTable

}	// namespace ro

#endif	// __roObjectTable_h__
