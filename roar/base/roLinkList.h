#ifndef __roLinklist_h__
#define __roLinklist_h__

#include "roNonCopyable.h"
#include "../platform/roCompiler.h"

namespace ro {

template<class Node> struct LinkList;

struct _LinkList;

struct _ListNode
{
	virtual ~_ListNode();
	virtual void destroyThis();
	virtual void removeThis();

	_LinkList* _list = NULL;
	_ListNode* _prev = NULL;
	_ListNode* _next = NULL;
};	// _ListNode

template<class T>
struct ListNode : public _ListNode
{
	bool			isInList() const		{ return _list != NULL; }
	LinkList<T>*	getList() const			{ return static_cast<LinkList<T>*>(_list); }
	T*				next() const			{ return (T*)_next; }
	T*				prev() const			{ return (T*)_prev; }

	template<class U>
	T*				next() const			{ return static_cast<T*>(static_cast<U*>(_next)); }

	void			removeThis() override	{ _ListNode::removeThis(); }
	void			destroyThis() override	{ _ListNode::destroyThis(); }
};	// ListNode

struct _LinkList : private NonCopyable
{
	_LinkList();
	~_LinkList();

	_ListNode* getAt(roSize i);

	void pushFront(_ListNode& newNode);
	void pushBack(_ListNode& newNode);
	void insertBefore(_ListNode& newNode, const _ListNode& beforeThis);
	void insertAfter(_ListNode& newNode, const _ListNode& afterThis);

	// Similar to the push/insert version, but will try to remove the node from any existing list first
	void moveFront(_ListNode& newNode);
	void moveBack(_ListNode& newNode);
	void moveBefore(_ListNode& newNode, const _ListNode& beforeThis);
	void moveAfter(_ListNode& newNode, const _ListNode& afterThis);

	void removeAll();
	void destroyAll();

	_ListNode* _head;	///< The head of the list (a dummy node)
	_ListNode* _tail;	///< The tail of the list (a dummy node)
	roSize _count;
};	// _LinkList

///	A wrapper class over the _LinkList to provide typed interface.
template<class Node_>
struct LinkList : public _LinkList
{
	typedef Node_ Node;

	LinkList() : _LinkList() {}

	using _LinkList::pushFront;
	using _LinkList::pushBack;
	using _LinkList::insertBefore;
	using _LinkList::insertAfter;

	using _LinkList::moveFront;
	using _LinkList::moveBack;
	using _LinkList::moveBefore;
	using _LinkList::moveAfter;

	Node*		getAt(roSize i)	{ return (Node*)(ListNode<Node>*)_LinkList::getAt(i); }

	roSize		size() const	{ return _count; }
	bool		isEmpty() const	{ return _count == 0; }

	Node&		front()			{ roAssert(_count > 0); return (Node&)(ListNode<Node>&)(*_head->_next); }
	const Node&	front() const	{ roAssert(_count > 0); return (Node&)(ListNode<Node>&)(*_head->_next); }
	Node&		back()			{ roAssert(_count > 0); return (Node&)(ListNode<Node>&)(*_tail->_prev); }
	const Node&	back() const	{ roAssert(_count > 0); return (Node&)(ListNode<Node>&)(*_tail->_prev); }

	Node*		begin()			{ return (Node*)(ListNode<Node>*)_head->_next; }
	const Node*	begin() const	{ return (Node*)(ListNode<Node>*)_head->_next; }
	Node*		rbegin()		{ return (Node*)(ListNode<Node>*)_tail->_prev; }
	const Node*	rbegin() const	{ return (Node*)(ListNode<Node>*)_tail->_prev; }
	Node*		end()			{ return (Node*)(ListNode<Node>*)_tail; }
	const Node*	end() const		{ return (Node*)(ListNode<Node>*)_tail; }
	Node*		rend()			{ return (Node*)(ListNode<Node>*)_head; }
	const Node*	rend() const	{ return (Node*)(ListNode<Node>*)_head; }
};	// LinkList

}	// namespace ro

#endif	// __roLinklist_h__

#include "roLinkList.inl"
