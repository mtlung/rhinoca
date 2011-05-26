#include "pch.h"
#include "document.h"
#include "body.h"
#include "element.h"
#include "nodelist.h"
#include "../common.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLDocument::jsClass = {
	"HTMLDocument", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getBody(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLDocument* self = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	HTMLBodyElement* body = self->body();
	if(!body)
		*vp = JSVAL_NULL;
	else
		*vp = *body;
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"body", 0, 0, getBody, JS_PropertyStub},
	{0}
};

static JSBool createElement(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	HTMLDocument* self = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	
	if(Element* ele = self->createElement(str))
	{
		ele->bind(cx, NULL);
		*rval = *ele;
	}
	else
		*rval = JSVAL_VOID;

	return JS_TRUE;
}

static JSBool getElementById(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	HTMLDocument* self = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	Element* ele = self->getElementById(str);

	*rval = *ele;

	return JS_TRUE;
}

static JSBool getElementsByTagName(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLDocument* self = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	toupper(str);
	*rval = *self->getElementsByTagName(str);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"createElement", createElement, 1,0,0},
	{"getElementById", getElementById, 1,0,0},
	{"getElementsByTagName", getElementsByTagName, 1,0,0},
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

static bool getElementsByTagNameFilter(NodeIterator& iter, void* userData)
{
	FixString s((rhuint32)userData);
	if(Element* ele = dynamic_cast<Element*>(iter.current(), iter.next())) {
		if(ele->tagName() == s)
			return true;
	}

	return false;
}

NodeList* HTMLDocument::getElementsByTagName(const char* tagName)
{
	NodeList* list = new NodeList(this, getElementsByTagNameFilter, (void*)StringHash(tagName, 0).hash);
	list->bind(jsContext, NULL);
	return list;
}

DOMWindow* HTMLDocument::window()
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
