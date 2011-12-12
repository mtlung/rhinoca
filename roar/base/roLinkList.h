#ifndef __roLinklist_h__
#define __roLinklist_h__

#include "roNonCopyable.h"

namespace ro {

template<class Node> struct LinkList;

struct _LinkList;

struct _ListNode
{
	_ListNode() : _list(NULL), _prev(NULL), _next(NULL) {}
	virtual ~_ListNode();
	virtual void destroyThis();
	void removeThis();

	_LinkList* _list;
	_ListNode* _prev;
	_ListNode* _next;
};	// _ListNode

template<class T>
struct ListNode : public _ListNode
{
	ListNode() {}

	bool			isInList() const	{ return _list != NULL; }
	LinkList<T>*	getList() const		{ return static_cast<LinkList<T>*>(_list); }
	T*				next() const		{ return (T*)_next; }
	T*				prev() const		{ return (T*)_prev; }

	void			removeThis()		{ _ListNode::removeThis(); }
	override void	destroyThis()		{ _ListNode::destroyThis(); }
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

	void removeAll();
	void destroyAll();

	_ListNode* _head;	///< The head of the list (a dummy node)
	_ListNode* _tail;	///< The tail of the list (a dummy node)
	roSize _count;
};	// _LinkList

///	A wrapper class over the _LinkList to provide typed interface.
template<class Node>
struct LinkList : public _LinkList
{
	typedef Node Node;

	LinkList() : _LinkList() {}

	using _LinkList::pushFront;
	using _LinkList::pushBack;
	using _LinkList::insertBefore;
	using _LinkList::insertAfter;

	Node*		getAt(roSize i)	{ return (Node*)_LinkList::getAt(i); }

	roSize		size() const	{ return _count; }
	bool		isEmpty() const	{ return _count > 0; }

	Node&		front()			{ roAssert(_count > 0); return (Node&)(*_head->_next); }
	const Node&	front() const	{ roAssert(_count > 0); return (Node&)(*_head->_next); }
	Node&		back()			{ roAssert(_count > 0); return (Node&)(*_tail->_prev); }
	const Node&	back() const	{ roAssert(_count > 0); return (Node&)(*_tail->_prev); }

	Node*		begin()			{ return (Node*)_head->_next; }
	const Node*	begin() const	{ return (Node*)_head->_next; }
	Node*		rbegin()		{ return (Node*)_tail->_prev; }
	const Node*	rbegin() const	{ return (Node*)_tail->_prev; }
	Node*		end()			{ return (Node*)_tail; }
	const Node*	end() const		{ return (Node*)_tail; }
	Node*		rend()			{ return (Node*)_head; }
	const Node*	rend() const	{ return (Node*)_head; }
};	// LinkList

}	// namespace ro

#endif	// __roLinklist_h__
