#ifndef __DOME_NODE_H__
#define __DOME_NODE_H__

#include "../jsbindable.h"
#include "../rhstring.h"

namespace Dom {

class HTMLDocument;

/// Reference: http://www.w3schools.com/dom/dom_node.asp
/// Reference: https://developer.mozilla.org/en/Gecko_DOM_Reference
class Node : public JsBindable
{
public:
	Node();

	/// Don't destruct a Node (or it's derived class) directly, it
	/// should be preformed by the GC.
	~Node();

// Operations
	void bind(JSContext* cx, JSObject* parent);
	JSObject* createPrototype();

	/// Appends a new child node to the end of the list of children.
	void appendChild(Node* child);

	/// Inserts a new child node before an existing child node.
	void insertBefore(Node* newChild, Node* refChild);

	/// Will not delete the C++ object, actual delete is done during JS GC.
	void removeThis();

// Attributes
	FixString nodeName;
	HTMLDocument* ownerDocument;

	Node* parentNode;
	Node* firstChild;
	Node* lastChild();
	Node* nextSibling;
	Node* previousSibling();

	bool hasChildNodes() const;

	static JSClass jsClass;
};	// Node

/*!	An iterator that preforms a pre-order traversal on the Node tree.
	Example:
	\code
	Node* root;
	for(NodeIterator itr(root); !itr.ended(); itr.next()) {
		// Do something ...
	}
	\endcode
 */
class NodeIterator
{
public:
	explicit NodeIterator(Node* start);

	explicit NodeIterator(Node* start, Node* current);

	/// NOTE: Assumming the iterator is valid and so the returned pointer will not be null.
	Node* operator->() { return _current; }

	/// Return the current element.
	Node* current() { return _current; }

	/// Returns true if there are NO more items in the collection.
	bool ended() const { return _current == NULL; }

	/// Returns the next element in the collection, and advances to the next.
	Node* next();

	///	Returns and advances to the next element in the collection, without visiting current node's children.
	Node* skipChildren();

protected:
	Node* _current;
	//! The position where this iterator is constructed, so it knows where to stop.
	const Node* _start;
};	// NodeIterator

}	// namespace Dom

#endif	// __DOME_NODE_H__
