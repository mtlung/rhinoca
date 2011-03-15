#ifndef __DOME_DOCUMENT_H__
#define __DOME_DOCUMENT_H__

#include "node.h"

namespace Dom {

class Element;
class DOMWindow;
class HTMLBodyElement;

// Reference: http://www.w3schools.com/dom/dom_document.asp
class HTMLDocument : public Node
{
public:
	explicit HTMLDocument(Rhinoca* rh);
	~HTMLDocument();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	Element* createElement(const char* eleType);

	Element* getElementById(const char* id);

// Attributes
	Rhinoca* rhinoca;

	DOMWindow* window();

	HTMLBodyElement* body();

	static JSClass jsClass;
};	// HTMLDocument

}	// namespace Dom

#endif	// __DOME_DOCUMENT_H__
