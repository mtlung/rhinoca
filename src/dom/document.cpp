#include "pch.h"
#include "document.h"
#include "body.h"
#include "element.h"
#include "keyevent.h"
#include "mouseevent.h"
#include "nodelist.h"
#include "textnode.h"
#include "../common.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLDocument::jsClass = {
	"HTMLDocument", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool createElement(JSContext* cx, uintN argc, jsval* vp)
{
	JsString jss(cx, JS_ARGV0);
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);
	
	if(Element* ele = self->createElement(jss.c_str()))
	{
		ele->bind(cx, NULL);
		JS_RVAL(cx, vp) = *ele;
	}
	else
		JS_RVAL(cx, vp) = JSVAL_VOID;

	return JS_TRUE;
}

static JSBool createTextNode(JSContext* cx, uintN argc, jsval* vp)
{
	JsString jss(cx, JS_ARGV0);
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);

	if(TextNode* node = self->createTextNode(jss.c_str()))
	{
		node->bind(cx, NULL);
		JS_RVAL(cx, vp) = *node;
	}
	else
		JS_RVAL(cx, vp) = JSVAL_VOID;

	return JS_TRUE;
}

static JSBool createDocumentFragment(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);

	// We simply use a node to represent a DocumentFragment
	Node* node = new Node(self->rhinoca);
	node->bind(cx, NULL);
	JS_RVAL(cx, vp) = *node;

	return JS_TRUE;
}

static JSBool getElementById(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;
	Element* ele = self->getElementById(jss.c_str());

	JS_RVAL(cx, vp) = *ele;

	return JS_TRUE;
}

static JSBool getElementsByTagName(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;
	toupper(jss.c_str());
	JS_RVAL(cx, vp) = *self->getElementsByTagName(jss.c_str());

	return JS_TRUE;
}

static JSBool createEvent(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, vp);

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	JS_RVAL(cx, vp) = *self->createEvent(jss.c_str());

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"createElement", createElement, 1,0},
	{"createTextNode", createTextNode, 1,0},
	{"createDocumentFragment", createDocumentFragment, 0,0},
	{"getElementsByTagName", getElementsByTagName, 1,0},
	{"getElementById", getElementById, 1,0},
	{"createEvent", createEvent, 1,0},
	{0}
};

static JSBool getBody(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);
	*vp = *self->body();
	return JS_TRUE;
}

// https://developer.mozilla.org/en/DOM/document.documentElement
static JSBool documentElement(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);
	// TODO: If the file is an XML, document.documentElement != document.firstChild
	*vp = *self->firstChild;
	return JS_TRUE;
}

static JSBool readyState(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);

	JS_RVAL(cx, vp) = STRING_TO_JSVAL(JS_InternString(cx, self->readyState));

	return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onmouseup",
	"onmousedown",
	"onmousemove",
	"onkeydown",
	"onkeyup"
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	*vp = self->getEventListenerAsAttribute(cx, _eventAttributeTable[idx]);
	return JS_TRUE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec properties[] = {
	{"body", 0, JSPROP_READONLY | JsBindable::jsPropFlags, getBody, JS_StrictPropertyStub},
	{"documentElement", 0, JSPROP_READONLY | JsBindable::jsPropFlags, documentElement, JS_StrictPropertyStub},
	{"readyState", 0, JSPROP_READONLY | JsBindable::jsPropFlags, readyState, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[3], 3, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[4], 4, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{0}
};

// Static variables to keep the intern string on life
static FixString _readyStateUninitialized = "uninitialized";
static FixString _readyStateLoading = "loading";
static FixString _readyStateInteractive = "interactive";
static FixString _readyStateComplete = "complete";

HTMLDocument::HTMLDocument(Rhinoca* rh)
	: Node(rh)
	, readyState(_readyStateUninitialized)
{
}

HTMLDocument::~HTMLDocument()
{
}

void HTMLDocument::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_DefineObject(cx, parent, "document", &jsClass, Node::createPrototype(), JSPROP_ENUMERATE);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));

	addReference();
}

Element* HTMLDocument::createElement(const char* eleType)
{
	ElementFactory& factory = ElementFactory::singleton();
	return factory.create(this->rhinoca, eleType, NULL);
}

TextNode* HTMLDocument::createTextNode(const char* data)
{
	TextNode* ret = new TextNode(rhinoca);
	ret->data = data;
	return ret;
}

Element* HTMLDocument::getElementById(const char* id)
{
	StringHash h(id, 0);
	for(NodeIterator i(this); !i.ended(); i.next()) {
		if(Element* e = dynamic_cast<Element*>(i.current())) {
			if(e->id.hashValue() == h)
				return e;
		}
	}
	return NULL;
}

static Node* getElementsByTagNameFilter(NodeIterator& iter, void* userData)
{
	FixString s((rhuint32)userData);
	Element* ele = dynamic_cast<Element*>(iter.current());
	iter.next();

	if(ele && ele->tagName() == s)
		return ele;

	return NULL;
}

NodeList* HTMLDocument::getElementsByTagName(const char* tagName)
{
	NodeList* list = new NodeList(this, getElementsByTagNameFilter, (void*)StringHash(tagName, 0).hash);
	list->bind(jsContext, NULL);
	return list;
}

Event* HTMLDocument::createEvent(const char* type)
{
	StringHash hash(type, 0);

	Event* e = NULL;

	if(hash == StringHash("MouseEvent") || hash == StringHash("MouseEvents"))
		e = new MouseEvent;
	else if(hash == StringHash("KeyEvents") || hash == StringHash("KeyboardEvent"))
		e = new KeyEvent;
	else if(hash == StringHash("HTMLEvents"))
		e = new Event;
	
	if(e)
		e->bind(jsContext, NULL);

	return e;
}

EventTarget* HTMLDocument::eventTargetTraverseUp()
{
	return rhinoca->domWindow;
}

Window* HTMLDocument::window()
{
	return rhinoca->domWindow;
}

HTMLBodyElement* HTMLDocument::body()
{
	for(NodeIterator i(this); !i.ended(); i.next()) {
		if(Element* e = dynamic_cast<Element*>(i.current())) {
			if(strcmp(e->tagName(), "BODY") == 0)
				return dynamic_cast<HTMLBodyElement*>(i.current());
		}
	}
	return NULL;
}

}	// namespace Dom
