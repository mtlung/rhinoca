#include "pch.h"
#include "document.h"
#include "body.h"
#include "element.h"
#include "keyevent.h"
#include "mouseevent.h"
#include "nodelist.h"
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

static JSBool getBody(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLDocument* self = getJsBindable<HTMLDocument>(cx, obj);
	*vp = *self->body();
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"body", 0, 0, getBody, JS_StrictPropertyStub},
	{0}
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
	{"getElementsByTagName", getElementsByTagName, 1,0},
	{"getElementById", getElementById, 1,0},
	{"createEvent", createEvent, 1,0},
	{0}
};

HTMLDocument::HTMLDocument(Rhinoca* rh)
	: rhinoca(rh)
{
	ownerDocument = this;
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

	e->bind(jsContext, NULL);
	return e;
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
