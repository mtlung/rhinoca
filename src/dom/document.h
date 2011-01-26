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
	explicit Document(Rhinoca* rh);
	~Document();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	Element* createElement(const char* eleType);

	Element* getElementById(const char* id);

// Attributes
	Rhinoca* rhinoca;
	Node* rootNode() { return _rootNode; }

	static JSClass jsClass;

protected:
	Node* _rootNode;
};	// Document

}	// namespace Dom

#endif	// __DOME_DOCUMENT_H__
