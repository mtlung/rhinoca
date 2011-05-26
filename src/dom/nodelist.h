#ifndef __DOME_NODELIST_H__
#define __DOME_NODELIST_H__

#include "Node.h"

namespace Dom {

class Node;

/// As state in the DOM reference, a NodeList is "live", that is, a NodeList object
/// won't store a snapshot of the tree, but store the root and a filter to query the
/// tree every time the we want to access it.
class NodeList : public JsBindable
{
public:
	typedef Node* (*Filter)(NodeIterator& iter, void* userData);
	explicit NodeList(Node* node, Filter filter=NULL, void* fileterUserData=NULL);

	~NodeList();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	Node* item(unsigned index);

// Attributes
	unsigned length();

	Node* root;

	Filter filter;
	void* userData;

	static JSClass jsClass;
};	// NodeList

}	// namespace Dom

#endif	// __DOME_NODELIST_H__
