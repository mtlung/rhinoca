#ifndef ___MAP_H__
#define ___MAP_H__

#include "rhassert.h"
#include "noncopyable.h"

/*!	\class MapBase
	The base class for an intrusive associative map container.
	The elements in the map are sorted and the sorting criteria is base on the Map::Comparator object,
	see MapComparator for more details.

	To use the map, user have to extend the node first:
	\code
	struct FooNode : public MapBase<int>::Node<FooNode> {
		typedef MapBase<int>::Node<FooNode> Super;
		FooNode(int key, const std::string& val)
			: Super(key), mVal(val)
		{}
		std::string mVal;
	};	// FooNode

	// ...

	Map<FooNode> map;
	map.insert(*(new FooNode(1, "1")));
	\endcode

	To iterate tough the elements of the map:
	\code
	// Iterator from small to large
	for(FooNode* n = map.findMin(); n != NULL; n = n->next()) {}
	// Iterator from large to small
	for(FooNode* n = map.findMax(); n != NULL; n = n->prev()) {}
	\endcode

	To find an element with a provided key:
	\code
	FooNode* n = map.find(1);
	if(n != NULL)
		std::cout << n->mVal << std::endl;
	else
		std::cout << "Key not found!" << std::endl;
	\endcode

	To add an element into multiple map (or similar intrusive container such as LinkList):
	\code
	// Node for a bi-directional map
	struct BiDirMapNode {
		BiDirMapNode(int id, const std::string& str)
			: mId(id), mStr(str)
		{}

		struct Integer : public MapBase<int>::NodeBase {
			explicit Integer(int key) : NodeBase(key) {}
			MCD_DECLAR_GET_OUTER_OBJ(BiDirMapNode, mId);
			void destroyThis() {
				delete getOuterSafe();
			}
		} mId;

		struct String : public MapBase<std::string>::NodeBase {
			explicit String(const std::string& key) : NodeBase(key) {}
			MCD_DECLAR_GET_OUTER_OBJ(BiDirMapNode, mStr);
			void destroyThis() {
				delete getOuterSafe();
			}
		} mStr;
	};	// BiDirMapNode

	// ...

	Map<BiDirMapNode::Integer> idToStr;
	Map<BiDirMapNode::String> strToId;

	BiDirMapNode* node = new BiDirMapNode(123, "123");
	idToStr.insert(node->mId);
	strToId.insert(node->mStr);

	BiDirMapNode* p1 = idToStr.find(123).getOuterSafe();
	BiDirMapNode* p2 = strToId.find("123").getOuterSafe();
	RHASSERT(p1 == p2);
	\endcode
 */

namespace Impl {

/*!	The core Avl tree implementation for the Map class.
	It is a non template class thus save code size for the Map class.
	\sa http://www.codeproject.com/KB/recipes/Containers.aspx
 */
class AvlTree : Noncopyable
{
protected:
	class Node
	{
		friend class AvlTree;

	protected:
		enum Direction { Left = 0, Right = 1 };

		Node();

		virtual ~Node();

	public:
		virtual void destroyThis();

		void removeThis();

	protected:
		bool isInMap() const {
			return mAvlTree != NULL;
		}

		AvlTree* getMap() {
			return mAvlTree;
		}

		Node* next() {
			if(mAvlTree)
				return mAvlTree->walkR(*this);
			return NULL;
		}

		Node* prev() {
			if(mAvlTree)
				return mAvlTree->walkL(*this);
			return NULL;
		}

		bool isBallanceOk() const;

		//! Get weather this node is left or right child of it's parent.
		Direction parentIdx() const;

		//! Set \em child as the child of this node. Note that \em child can be null.
		void setChildSafe(Direction dir, Node* child);

#ifndef NDEBUG
		size_t assertValid(size_t& total, const Node* parent, size_t nL, size_t nR) const;
		Node* mRight;	//! For visual studio debug visualization
#endif	// NDEBUG

		void synLeftRight();	//! For visual studio debug visualization

		Node* mChildren[2];	// Left/Right children
		Node* mParent;
		AvlTree* mAvlTree;
		int mBallance;	// Negative=Left, Positive=Right
	};	// Node

	enum Direction { Left = Node::Left, Right = Node::Right };

	AvlTree();

	void insert(Node& node, Node* parent, int nIdx);

	void remove(Node& node, Node* onlyChild);

	void remove(Node& node);

	void replaceFixTop(Node& node, Node& next);

	bool rotate(Node& node, int dir);

	void adjustBallance(Node& node, int dir, bool removed);

	//! Get the extreme leaf node.
	template<Direction dir>	static Node* getExtreme(Node& pos);
	static Node* getExtremeL(Node& pos);
	static Node* getExtremeR(Node& pos);

	Node* getExtrRootL() {
		return mRoot ? getExtremeL(*mRoot) : NULL;
	}
	Node* getExtrRootR() {
		return mRoot ? getExtremeR(*mRoot) : NULL;
	}

	//! Perform in-order traversal.
	template<Direction dir>
	Node* walk(Node& node);

	//! Reversed in-order traversal (small to large).
	Node* walkL(Node& node);

	//! Forward in-order traversal (large to small).
	Node* walkR(Node& node);

public:
	~AvlTree();

	//!	Remove all elements
	void removeAll();

	//! Destroy all elements
	void destroyAll();

	size_t elementCount() const {
		return mCount;
	}

	bool isEmpty() const;

protected:
	Node* mRoot;
	size_t mCount;
};	// AvlTree

}	// namespace Impl

/*!	The basic comparator used in MCD::Map.
	Requires \em TKeyArg to have the < and == operators defined.
	User can make explicit template specialization on different types of \em TKeyArg,
	see MapCharStrComparator for a specialization for string as key type.

	\sa MCD::Map
 */
template<typename TKeyArg>
struct MapComparator {
	typedef TKeyArg KeyArg;

	explicit MapComparator(KeyArg key) : mKey(key) {}

	/*!	Returns:
		- if \em key <  \em mKey
		0 if \em key == \em mKey
		+ if \em key >  \em mKey
	 */
	int compare(KeyArg key) {
		return (key < mKey) ? (-1) : (key == mKey) ? 0 : 1;
	}

	// Using \em KeyArg as the type is enough, since the scope of \em MapComparator is temporary.
	KeyArg mKey;
};	// MapComparator

/// Specialized comparator for int type
template<> struct MapComparator<const int> {
	explicit MapComparator(int key) : mKey(key) {}
	int compare(int key) { return key - mKey; }
	int mKey;
};	// MapComparator

template<> struct MapComparator<const unsigned> {
	explicit MapComparator(int key) : mKey(key) {}
	int compare(int key) { return key - mKey; }
	int mKey;
};	// MapComparator

template<
	class TKey,	//! The key type
	class TKeyArg = const TKey,
	class TComparator = MapComparator<TKeyArg>
>
class MapBase : public Impl::AvlTree
{
public:
	//! The key type.
	typedef TKey Key;

	//! The type for passing \em Key as function argument.
	typedef TKeyArg KeyArg;

	//! The comparator object type.
	typedef TComparator Comparator;

	/*!	A single node of the MapBase container.
		\note Each node consume 5 * sizeof(void*) that means 20 bytes on 32-bit machine.
	 */
	class NodeBase : public Impl::AvlTree::Node
	{
	public:
		typedef TKey Key;
		typedef TKeyArg KeyArg;

		NodeBase() {}
		explicit NodeBase(KeyArg key) : mKey(key) {}

		/*!	Destroy the node itself.
			By default it will call "delete this", user can override
			this function for their own memory management.
		 */
		virtual void destroyThis() {
			Impl::AvlTree::Node::destroyThis();
		}

		//! Remove this node from the map, if it's already in the map.
		void removeThis() {
			Impl::AvlTree::Node::removeThis();
		}

		bool isInMap() const {
			return Impl::AvlTree::Node::isInMap();
		}

		//! Returns null if the node is not in any map.
		MapBase* getMap() {
			return static_cast<MapBase*>(Impl::AvlTree::Node::getMap());
		}
		const MapBase* getMap() const {
			return const_cast<NodeBase*>(this)->getMap();
		}

		//! Get the next node (with key >= this key), return null if this is the last node.
		NodeBase* next() {
			return static_cast<NodeBase*>(Node::next());
		}
		const NodeBase* next() const {
			return const_cast<NodeBase*>(this)->next();
		}

		//! Get the previous node (with key <= this key), return null if this is the first node.
		NodeBase* prev() {
			return static_cast<NodeBase*>(Node::prev());
		}
		const NodeBase* prev() const {
			return const_cast<NodeBase*>(this)->prev();
		}

		KeyArg getKey() const {
			return mKey;
		}

		//! Reassign the key value. The map will be adjusted to keep itself strictly sorted.
		void setKey(KeyArg key) {
			MapBase* map = getMap();
			removeThis();
			mKey = key;
			if(map)
				map->insert(*this);
		}

	private:
		friend class MapBase;
#ifndef NDEBUG
		size_t assertValid(size_t& total, const NodeBase* parent, int dir) const
		{
			if(!this)
				return 0;

			if(parent) {
				Comparator hint(parent->mKey);
				int cmp = hint.compare(mKey);
				bool sameSign = cmp < 0 && dir < 0;
				RHASSERT(!cmp || sameSign);
			}

			size_t nL = static_cast<NodeBase*>(mChildren[Left])->assertValid(total, this, -1);
			size_t nR = static_cast<NodeBase*>(mChildren[Right])->assertValid(total, this, 1);
			return Impl::AvlTree::Node::assertValid(total, parent, nL, nR);
		}
#endif	// NDEBUG

		Key mKey;
	};	// NodeBase

	//! A wrapper class over the NodeBase to provide typed interface.
	template<class TNodeType>
	class Node : public NodeBase
	{
	public:
		typedef TNodeType NodeType;

		Node() {}
		explicit Node(KeyArg key) : NodeBase(key) {}

		NodeType* next() {
			return static_cast<NodeType*>(NodeBase::next());
		}
		const NodeType* next() const {
			return const_cast<Node*>(this)->next();
		}

		NodeType* prev() {
			return static_cast<NodeType*>(NodeBase::prev());
		}
		const NodeType* prev() const {
			return const_cast<Node*>(this)->prev();
		}
	};	// Node

	void insert(NodeBase& newNode)
	{
		if(mRoot) {
			Comparator hint(newNode.mKey);
			for(NodeBase* node = static_cast<NodeBase*>(mRoot); ;) {
				int nIdx = (hint.compare(node->mKey) < 0);
				if(!node->mChildren[nIdx]) {
					AvlTree::insert(newNode, node, nIdx);
					break;
				}
				node = static_cast<NodeBase*>(node->mChildren[nIdx]);
			}
		}
		else
			AvlTree::insert(newNode, NULL, 0);
	}

	//! This function will ensure the insertion will not create more duplicated key.
	bool insertUnique(NodeBase& newNode)
	{
		if(find<true, 0>(newNode.getKey()) != NULL)
			return false;
		insert(newNode);
		return true;
	}

	/*!	Remove all elements in the map.
		To remove a single element, use MapBase::Node::RemoveFromList
	 */
	void removeAll() {
		Impl::AvlTree::removeAll();
	}

	void assertValid() const
	{
#ifndef NDEBUG
		size_t nTotal = 0;
		static_cast<NodeBase*>(mRoot)->assertValid(nTotal, NULL, 0);
		RHASSERT(nTotal == elementCount());
#endif	// NDEBUG
	}

protected:
	template<bool Exact, int Dir>
	NodeBase* find(KeyArg key)
	{
		Comparator hint(key);
		NodeBase* match = NULL;

		for(NodeBase* node = static_cast<NodeBase*>(mRoot); node;) {
			const int cmp = hint.compare(node->mKey);
			const int ltz = cmp < 0;

			if(Exact && cmp == 0)
				return node;	// Exact match.
			else if(Dir < 0 && ltz)
				match = node;

			node = static_cast<NodeBase*>(node->mChildren[ltz]);
		}
		return match;
	}
};	// MapBase

/*!	A wrapper class over the MapBase to provide typed interface.
	\sa MapBase
 */
template<class TNode, class TComparator = MapComparator<typename TNode::KeyArg> >
class Map : public MapBase<typename TNode::Key, typename TNode::KeyArg, TComparator>
{
public:
	typedef TNode Node;
	typedef MapBase<typename TNode::Key, typename TNode::KeyArg, TComparator> Super;
	typedef typename Super::KeyArg KeyArg;
	typedef typename Super::Comparator Comparator;

	Node*		find(KeyArg key)					{ return static_cast<Node*>(Super::find<true, 0>(key)); }
	const Node*	find(KeyArg key) const				{ return const_cast<Map*>(this)->find(key); }

	Node*		findMin()							{ return static_cast<Node*>(Super::getExtrRootL()); }
	const Node*	findMin(KeyArg key) const			{ return const_cast<Map*>(this)->findMin(key); }
	Node*		findMax()							{ return static_cast<Node*>(Super::getExtrRootR()); }
	const Node*	findMax(KeyArg key) const			{ return const_cast<Map*>(this)->findMax(key); }

	Node*		findSmaller(KeyArg key)				{ return static_cast<Node*>(Super::find<false, -1>(key)); }
	const Node*	findSmaller(KeyArg key) const		{ return const_cast<Map*>(this)->findSmaller(key); }
	Node*		findBigger(KeyArg key)				{ return static_cast<Node*>(Super::find<false, 1>(key)); }
	const Node*	findBigger(KeyArg key) const		{ return const_cast<Map*>(this)->findBigger(key); }

	Node*		findExactSmaller(KeyArg key)		{ return static_cast<Node*>(Super::find<true, -1>(key)); }
	const Node*	findExactSmaller(KeyArg key) const	{ return const_cast<Map*>(this)->findExactSmaller(key); }
	Node*		findExactBigger(KeyArg key)			{ return static_cast<Node*>(Super::find<true, 1>(key)); }
	const Node*	findExactBigger(KeyArg key) const	{ return const_cast<Map*>(this)->findExactBigger(key); }

	void		insert(Node& newNode)				{ Super::insert(newNode); }
	bool		insertUnique(Node& newNode)			{ return Super::insertUnique(newNode); }
};	// Map

#endif	// ___MAP_H__
