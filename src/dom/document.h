#ifndef __DOM_DOCUMENT_H__
#define __DOM_DOCUMENT_H__

#include "node.h"
#include "cssstylesheet.h"

namespace Dom {

class Element;
class Event;
class HTMLBodyElement;
class NodeList;
class TextNode;
class Window;

/// Reference: http://www.w3schools.com/dom/dom_document.asp
class HTMLDocument : public Node
{
public:
	explicit HTMLDocument(Rhinoca* rh);
	~HTMLDocument();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	Element* createElement(const char* eleType);

	TextNode* createTextNode(const char* data);

	Element* getElementById(const char* id);

	NodeList* getElementsByTagName(const char* tagName);

	/// See https://developer.mozilla.org/En/DOM/Document.createEvent for list of valid type
	Event* createEvent(const char* type);

protected:
	override EventTarget* eventTargetTraverseUp();

public:
// Attributes
	Window* window();

	HTMLBodyElement* body();

	/// See http://www.w3schools.com/jsref/prop_doc_readystate.asp
	ro::ConstString readyState;

	LinkList<CSSStyleSheet> styleSheets;

	static JSClass jsClass;
};	// HTMLDocument

}	// namespace Dom

#endif	// __DOM_DOCUMENT_H__
