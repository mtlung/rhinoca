#include "pch.h"
#include "roLinklist.h"

namespace ro {

_ListNode::~_ListNode()
{
	removeThis();
}

void _ListNode::destroyThis()
{
	delete this;
}

void _ListNode::removeThis()
{
	if(!_list)
		return;

	--(_list->_count);

	_prev->_next = _next;
	_next->_prev = _prev;

	_list = NULL;
	_next = _prev = NULL;
}

_LinkList::_LinkList()
	: _head(new _ListNode), _tail(new _ListNode), _count(0)
{
	_head->_next = _tail;
	_tail->_prev = _head;
}

_LinkList::~_LinkList()
{
	destroyAll();
	delete _head;
	delete _tail;
}

void _LinkList::insertBefore(_ListNode& newNode, const _ListNode& beforeThis)
{
	roAssert(!newNode._list);
	roAssert("Parameter 'beforeThis' should be in this list" &&
		beforeThis._next == NULL || beforeThis._list == this
	);

	beforeThis._prev->_next = &newNode;
	newNode._prev = beforeThis._prev;

	const_cast<_ListNode&>(beforeThis)._prev = &newNode;
	newNode._next = const_cast<_ListNode*>(&beforeThis);

	newNode._list = this;
	++_count;
}

_ListNode* _LinkList::getAt(roSize index)
{
	if(index >= _count) return NULL;

	_ListNode* n = _head->_next;
	for(; n != _tail && index--; n = n->_next) {}
	return n;
}

void _LinkList::pushFront(_ListNode& newNode)
{
	insertAfter(newNode, *_head);
}

void _LinkList::pushBack(_ListNode& newNode)
{
	insertBefore(newNode, *_tail);
}

void _LinkList::insertAfter(_ListNode& newNode, const _ListNode& afterThis)
{
	insertBefore(newNode, *afterThis._next);
}

void _LinkList::removeAll()
{
	_ListNode* n;
	// Excluding the dummy node _head and _tail
	while(_tail != (n = _head->_next)) {
		n->removeThis();
	}
}

void _LinkList::destroyAll()
{
	_ListNode* n;
	// Excluding the dummy node _head and _tail
	while(_tail != (n = _head->_next)) {
		n->destroyThis();
	}
}

}	// namespace ro
