#ifndef __roMap_h__
#define __roMap_h__

#include "roNonCopyable.h"
#include "../platform/roCompiler.h"

namespace ro {

/// An intrusive associative map container.
/// More documentation on http://www.codeproject.com/KB/recipes/Containers.aspx
/*!	To use the map, user have to extend the node first:
\code
struct FooNode : public MapNode<int, FooNode> {
	typedef MapNode<int>::Node<FooNode> Super;
	FooNode(int key, const String& val)
		: Super(key), _val(val)
	{}
	String _val;
};	// FooNode

Map<FooNode> map;
map.insert(*(new FooNode(1, "1")));

// Iterator from small to large
for(FooNode* n = map.findMin(); n != NULL; n = n->next()) {}
// Iterator from large to small
for(FooNode* n = map.findMax(); n != NULL; n = n->prev()) {}
\endcode
 */
template<class Node> struct Map;

struct _AvlTree;

struct _TreeNode
{
	_TreeNode() : _tree(NULL) {}
	virtual ~_TreeNode();
	virtual void destroyThis();

#if roCOMPILER_VC && roDEBUG
	_TreeNode* mRight;	///< For visual studio debug visualization
#endif

	_AvlTree* _tree;
	_TreeNode* _parent;
	_TreeNode* _children[2];	///< Left/Right children
	roInt8 _balance;			///< Negative=Left, Positive=Right
};	// _TreeNode

template<class Key, class T, class Comparator = MapComparator<Key> >
struct MapNode : public _TreeNode
{
	typedef Key Key;
	typedef Comparator Comparator;

	MapNode() {}
	explicit MapNode(const Key& key) : _key(key) {}

	bool			isInMap() const	{ return _tree != NULL; }
	T*				next() const	{ return _tree ? (T*)(_tree->walkR((_TreeNode&)*this)) : NULL; }
	T*				prev() const	{ return _tree ? (T*)(_tree->walkL((_TreeNode&)*this)) : NULL; }

	const Key&		key() const		{ return _key; }
	void			setKey(const Key& key);

	void			removeThis();
	override void	destroyThis()	{ _TreeNode::destroyThis(); }

	Key _key;
};	// MapNode

struct _AvlTree : NonCopyable
{
	enum Direction { Left = 0, Right = 1 };

	_AvlTree();
	~_AvlTree();

	void insert			(_TreeNode& node, _TreeNode* parent, int nIdx);
	void remove			(_TreeNode& node, _TreeNode* onlyChild);
	void remove			(_TreeNode& node);
	bool rotate			(_TreeNode& node, roInt8 dir);
	void replaceFixTop	(_TreeNode& node, _TreeNode& next);
	void adjustBallance	(_TreeNode& node, roInt8 dir, bool removed);

	_TreeNode* leftMostChild() const;
	_TreeNode* rightMostChild() const;

	_TreeNode* walkL(_TreeNode& node);	/// Reversed in-order traversal (small to large).
	_TreeNode* walkR(_TreeNode& node);	/// Forward in-order traversal (large to small).

	void removeAll();
	void destroyAll();

	_TreeNode* _root;
	size_t _count;
};	// _AvlTree

template<class Node>
struct Map : public _AvlTree
{
	typedef _AvlTree Super;
	typedef typename Node::Key Key;
	typedef typename Node::Comparator Comparator;

	Node*		find				(Key key)		{ return _find<true, 0>(key); }
	const Node*	find				(Key key) const	{ return _find<true, 0>(key); }

	Node*		findMin				()				{ return static_cast<Node*>(Super::leftMostChild()); }
	const Node*	findMin				(Key key) const	{ return static_cast<Node*>(Super::leftMostChild()); }
	Node*		findMax				()				{ return static_cast<Node*>(Super::rightMostChild()); }
	const Node*	findMax				(Key key) const	{ return static_cast<Node*>(Super::rightMostChild()); }

	Node*		findSmaller			(Key key)		{ return _find<false, -1>(key); }
	const Node*	findSmaller			(Key key) const	{ return _find<false, -1>(key); }
	Node*		findBigger			(Key key)		{ return _find<false,  1>(key); }
	const Node*	findBigger			(Key key) const	{ return _find<false,  1>(key); }

	Node*		findExactSmaller	(Key key)		{ return _find<true, -1>(key); }
	const Node*	findExactSmaller	(Key key) const	{ return _find<true, -1>(key); }
	Node*		findExactBigger		(Key key)		{ return _find<true,  1>(key); }
	const Node*	findExactBigger		(Key key) const	{ return _find<true,  1>(key); }

	void		insert(Node& newNode);
	bool		insertUnique(Node& newNode);

// Private
	template<bool Exact, int Dir>
	Node*		_find(const Key& key) const;
};	// Map


// ----------------------------------------------------------------------

template<class Key>
struct MapComparator
{
	MapComparator(const Key& key) : _key(key) {}

	/*!	Returns:
		- if \em key <  \em _key
		0 if \em key == \em _key
		+ if \em key >  \em _key
	 */
	int compare(const Key& key) {
		return (key < _key) ? (-1) : (key == _key) ? 0 : 1;
	}

	const Key& _key;	/// Using reference as the type is ok

	void operator=(const MapComparator&);	/// Shut up assignment operator warning
};	// MapComparator

/// Specialized comparator for int type
template<> struct MapComparator<const int>
{
	MapComparator(int key) : _key(key) {}
	int compare(int key) { return key - _key; }
	int _key;
};	// MapComparator

template<> struct MapComparator<const unsigned>
{
	MapComparator(int key) : _key(key) {}
	int compare(int key) { return key - _key; }
	int _key;
};	// MapComparator


// ----------------------------------------------------------------------

template<class K, class T, class C>
void MapNode<K,T,C>::setKey(const K& key)
{
	Map<MapNode<K,T,C> >* map = (Map<MapNode<K,T,C> >*)_tree;
	removeThis();
	_key = key;
	if(map)
		map->insert(*this);
}

template<class K, class T, class C>
void MapNode<K,T,C>::removeThis()
{
	if(_tree)
		_tree->remove(*this);
	_tree = NULL;
}

template<class N>
void Map<N>::insert(N& newNode)
{
	if(_root) {
		typename N::Comparator hint(newNode._key);
		for(N* node = static_cast<N*>(_root); ;) {
			const int nIdx = (hint.compare(node->_key) < 0);
			if(!node->_children[nIdx]) {
				_AvlTree::insert(newNode, node, nIdx);
				break;
			}
			node = static_cast<N*>(node->_children[nIdx]);
		}
	}
	else
		_AvlTree::insert(newNode, NULL, 0);
}

template<class N>
bool Map<N>::insertUnique(N& newNode)
{
	if(_find<true, 0>(newNode.key()) != NULL)
		return false;
	insert(newNode);
	return true;
}

template<class N> template<bool Exact, int Dir>
N* Map<N>::_find(const typename N::Key& key) const
{
	typename N::Comparator hint(key);
	N* match = NULL;

	for(N* node = static_cast<N*>(_root); node;) {
		const int cmp = hint.compare(node->_key);
		const int ltz = cmp < 0;

		if(Exact && cmp == 0)
			return node;	// Exact match.
		else if(Dir < 0 && ltz)
			match = node;

		node = static_cast<N*>(node->_children[ltz]);
	}
	return match;
}

}	// namespace ro

#endif	// __roMap_h__
