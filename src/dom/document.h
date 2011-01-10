#ifndef __DOME_DOCUMENT_H__
#define __DOME_DOCUMENT_H__

#include "../jsbindable.h"

namespace Dom {

class Node;
class Element;

// Reference: http://www.w3schools.com/dom/dom_document.asp
class Document : public JsBindable
{
public:
	Document();
	~Document();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	Element* getElementById(const char* id);

// Attributes
	Node* rootNode() { return _rootNode; }

	static JSClass jsClass;

protected:
	Node* _rootNode;
};	// Document

}	// namespace Dom

#endif	// __DOME_DOCUMENT_H__
