#include "pch.h"
#include "roMap.h"
#include "roStringUtility.h"

namespace ro {

_TreeNode::~_TreeNode()
{
	if(!_tree)
		return;

	_tree->remove(*this);
	_tree = NULL;
}

void _TreeNode::destroyThis()
{
	delete this;
}

_AvlTree::_AvlTree()
	: _root(NULL), _count(0)
{
}

_AvlTree::~_AvlTree()
{
	destroyAll();
}

typedef _AvlTree::Direction Direction;
typedef _TreeNode Node;
static const Direction Left = _AvlTree::Left;
static const Direction Right = _AvlTree::Right;

#if roCOMPILER_VC && roDEBUG
static void _synLeftRight(_TreeNode& self) { self.mRight = self._children[Right]; }
#else
static void _synLeftRight(_TreeNode& self) {}
#endif	// NDEBUG

static Direction _parentIdx(const _TreeNode& self)
{
	roAssert(
		(&self == self._parent->_children[Left]) ||
		(&self == self._parent->_children[Right])
	);
	return &self == self._parent->_children[Left] ? Left : Right;
}

template<Direction dir>
static _TreeNode* _getExtreme(_TreeNode& pos_)
{
	_TreeNode* pos = &pos_;
	while(true) {
		_TreeNode* pVal = pos->_children[dir];
		if(!pVal)
			return pos;
		pos = pVal;
	}
}

static void _setChildSafe(_TreeNode& self, Direction dir, _TreeNode* child)
{
	if((self._children[dir] = child) != NULL)
		child->_parent = &self;

	_synLeftRight(self);
}

static bool _isBallanceOk(const _TreeNode& self)
{
	return (self._balance >= -1) && (self._balance <= 1);
}


void _AvlTree::insert(Node& node, Node* parent, int nIdx)
{
	if(node._tree == this)
		return;

	roAssert(!node._tree && "Remove the node from one container before adding to another");

	node._children[Left] = NULL;
	node._children[Right] = NULL;
	_synLeftRight(node);

	node._parent = parent;
	node._tree = this;
	node._balance = 0;

	if(parent) {
		roAssert(!parent->_children[nIdx]);
		parent->_children[nIdx] = &node;
		_synLeftRight(*parent);

		adjustBallance(*parent, nIdx ? 1 : -1, false);
	} else {
		roAssert(!_root);
		_root = &node;
	}

	++_count;
}

void _AvlTree::remove(Node& node, Node* onlyChild)
{
	roAssert(!node._children[Left] || !node._children[Right]);

	if(onlyChild)
		onlyChild->_parent = node._parent;

	Node* parent = node._parent;
	if(parent) {
		Direction idx = _parentIdx(node);
		parent->_children[idx] = onlyChild;
		_synLeftRight(*parent);

		adjustBallance(*parent, idx ? -1 : 1, true);
	} else
		_root = onlyChild;

	roAssert(_count > 0);
	--_count;
}

void _AvlTree::remove(Node& node)
{
	if(node._children[Left])
		if(node._children[Right]) {
			// Find the successor of this node.
			Node* pSucc = _getExtreme<Left>(*node._children[Right]);

			remove(*pSucc, pSucc->_children[Right]);

			_setChildSafe(*pSucc, Left, node._children[Left]);
			_setChildSafe(*pSucc, Right, node._children[Right]);
			replaceFixTop(node, *pSucc);

			pSucc->_balance = node._balance;
		} else
			remove(node, node._children[Left]);
	else
		remove(node, node._children[Right]);
}

void _AvlTree::removeAll()
{
	// NOTE: The simplest way, but slow because
	// all the tree balancing act is just a waste.
//	while(!isEmpty())
//		_root->removeThis();

	// A fast removeAll approach
	for(Node* n = leftMostChild(); n != NULL; ) {
		Node* bk = n;
		n = walkR(*n);
		bk->_tree = NULL;
	}
	_root = NULL;
	_count = 0;
}

void _AvlTree::destroyAll()
{
	// NOTE: The simplest way, but slow because
	// all the tree balancing act is just a waste.
	while(_root)
		_root->destroyThis();
}

void _AvlTree::replaceFixTop(Node& node, Node& next)
{
	if((next._parent = node._parent) != NULL) {
		node._parent->_children[_parentIdx(node)] = &next;
		_synLeftRight(*node._parent);
	}
	else {
		roAssert(&node == _root);
		_root = &next;
	}
}

bool _AvlTree::rotate(Node& node, roInt8 dir)
{
	roAssert((-1 == dir) || (1 == dir));
	int nIdx = (Right == dir);

	roAssert(node._balance);
	roAssert(node._balance * dir < 0);

	Node* pNext = node._children[!nIdx];
	roAssert(pNext != NULL);
	roAssert(_isBallanceOk(*pNext));

	if(dir == pNext->_balance) {
		roVerify(!rotate(*pNext, -dir));
		pNext = pNext = node._children[!nIdx];
		roAssert(pNext && _isBallanceOk(*pNext));
		roAssert(dir != pNext->_balance);
	}

	const bool bDepthDecrease = pNext->_balance && !_isBallanceOk(node);

	node._balance += dir;
	if(pNext->_balance) {
		if(!node._balance)
			pNext->_balance += dir;
		node._balance += dir;
	}
	pNext->_balance += dir;

	_setChildSafe(node, Direction(!nIdx), pNext->_children[nIdx]);

	pNext->_children[nIdx] = &node;
	_synLeftRight(*pNext);

	replaceFixTop(node, *pNext);

	node._parent = pNext;

	return bDepthDecrease;
}

void _AvlTree::adjustBallance(Node& node_, roInt8 dir, bool removed)
{
	roAssert((1 == dir) || (-1 == dir));
	Node* node = &node_;

	while(true) {
		roAssert(_isBallanceOk(*node));

		Node* parent = node->_parent;
		node->_balance += dir;

		roInt8 nDirNext = -1;
		if(parent)
			nDirNext = ((Left != Direction(_parentIdx(*node))) ^ removed) ? 1 : -1;

		bool match = false;
		switch(node->_balance) {
		case -1:
		case 1:
			match = false;
			break;
		case 0:
			match = true;
			break;
		case -2:
			match = rotate(*node, 1);
			break;
		case 2:
			match = rotate(*node, -1);
			break;
		default:
			roAssert(false);
		}

		if(!parent || (match ^ removed))
			break;

		node = parent;
		dir = nDirNext;
	}
}

template<Direction dir>
static Node* _walk(Node& node)
{
	Node* ret = node._children[dir];
	if(ret)
		return _getExtreme<dir == Left ? Right : Left>(*ret);

	ret = &node;
	while(true) {
		Node* parent = ret->_parent;
		if(!parent)
			return NULL;

		if(Direction(_parentIdx(*ret)) != dir)
			return parent;

		ret = parent;
	}
}

_TreeNode* _AvlTree::leftMostChild() const
{
	return _root ? _getExtreme<Left>(*_root) : NULL;
}

_TreeNode* _AvlTree::rightMostChild() const
{
	return _root ? _getExtreme<Right>(*_root) : NULL;
}

Node* _AvlTree::walkL(Node& node)
{
	return _walk<Left>(node);
}

Node* _AvlTree::walkR(Node& node)
{
	return _walk<Right>(node);
}

int MapComparator<const char*>::compare(const char* key)
{
	return roStrCmp(key, _key);
	return 0;
}

}	// namespace ro
