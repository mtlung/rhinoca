#ifndef __DOM_NODE_H__
#define __DOM_NODE_H__

#include "event.h"
#include "../rhinoca.h"
#include "../jsbindable.h"
#include "../../roar/base/roLinkList.h"

namespace Dom {

class NodeList;
class HTMLDocument;
class CanvasRenderingContext2D;

/// Reference: http://www.w3schools.com/dom/dom_node.asp
/// Reference: https://developer.mozilla.org/en/Gecko_DOM_Reference
class Node : public JsBindable, public EventTarget
{
public:
	explicit Node(Rhinoca* rh);

	/// Don't destruct a Node (or it's derived class) directly, it
	/// should be preformed by the GC.
	virtual ~Node();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;
	JSObject* createPrototype();

	/// Appends a new child node to the end of the list of children.
	/// If newChild is already in the tree, it is first removed.
	/// Returns newChild.
	Node* appendChild(Node* newChild);

	/// Inserts a new child node before an existing child node.
	/// If refChild is null, newChild is inserted at the end of.
	/// If newChild is already in the tree, it is first removed.
	/// Returns newChild on success.
	Node* insertBefore(Node* newChild, Node* refChild);

	/// Replace an existing child with another.
	/// If newChild is already in the tree, it is first removed.
	/// Returns oldChild on success.
	Node* replaceChild(Node* oldChild, Node* newChild);

	/// Returns the removed child on success.
	Node* removeChild(Node* child);

	/// Will not delete the C++ object, actual delete is done during JS GC.
	void removeThis();

	virtual Node* cloneNode(bool recursive);

	/// Render this node to the window
	virtual void render(CanvasRenderingContext2D* virtualCanvas) {}

	virtual void tick(float dt) {}

protected:
	JSObject* getJSObject() override { return jsObject; }
	EventTarget* eventTargetTraverseUp() override;
	void eventTargetAddReference() override;
	void eventTargetReleaseReference() override;

// Attributes
public:
	Rhinoca* rhinoca;
	ro::ConstString nodeName;

	HTMLDocument* ownerDocument() const;

	NodeList* childNodes();

	Node* parentNode;
	Node* firstChild;
	Node* lastChild();
	Node* nextSibling;
	Node* previousSibling();

	bool hasChildNodes() const;

	/// Only those node which need to tick every frame will put into the 'tick list'
	struct TickEntry : public ro::ListNode<Node::TickEntry> {
	} tickListNode;

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

	/// NOTE: Assuming the iterator is valid and so the returned pointer will not be null.
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

#endif	// __DOM_NODE_H__
