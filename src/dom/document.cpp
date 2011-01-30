#include "pch.h"
#include "document.h"
#include "body.h"
#include "element.h"
#include "node.h"
#include "../common.h"
#include "../context.h"

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
		*vp = OBJECT_TO_JSVAL(body->jsObject);
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
	HTMLDocument* doc = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	
	if(Element* ele = doc->createElement(str))
	{
		ele->bind(cx, NULL);
		*rval = OBJECT_TO_JSVAL(ele->jsObject);
	}
	else
		*rval = JSVAL_VOID;

	return JS_TRUE;
}

static JSBool getElementById(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	HTMLDocument* doc = reinterpret_cast<HTMLDocument*>(JS_GetPrivate(cx, obj));
	Element* ele = doc->getElementById(str);

	*rval = ele ? OBJECT_TO_JSVAL(ele->jsObject) : JSVAL_VOID;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"createElement", createElement, 1,0,0},
	{"getElementById", getElementById, 1,0,0},
	{0}
};

HTMLDocument::HTMLDocument(Rhinoca* rh)
	: rhinoca(rh)
	, _rootNode(NULL)
{
}

HTMLDocument::~HTMLDocument()
{
	_rootNode->removeThis();
}

void HTMLDocument::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_DefineObject(cx, parent, "document", &jsClass, 0, JSPROP_ENUMERATE);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));

	addReference();

	_rootNode = new Node;
	_rootNode->bind(jsContext, NULL);
	_rootNode->addGcRoot();	// releaseGcRoot() in ~HTMLDocument()
	_rootNode->ownerDocument = this;
}

Element* HTMLDocument::createElement(const char* eleType)
{
	ElementFactory& factory = ElementFactory::singleton();
	return factory.create(this->rhinoca, eleType, NULL);
}

Element* HTMLDocument::getElementById(const char* id)
{
	StringHash h(id, 0);
	for(NodeIterator i(_rootNode); !i.ended(); i.next()) {
		if(Element* e = dynamic_cast<Element*>(i.current())) {
			if(e->id.hashValue() == h)
				return e;
		}
	}
	return NULL;
}

DOMWindow* HTMLDocument::window()
{
	return rhinoca->domWindow;
}

HTMLBodyElement* HTMLDocument::body()
{
	for(NodeIterator i(_rootNode); !i.ended(); i.next()) {
		if(Element* e = dynamic_cast<Element*>(i.current())) {
			if(strcmp(e->tagName(), "BODY") == 0)
				return dynamic_cast<HTMLBodyElement*>(i.current());
		}
	}
	return NULL;
}

}	// namespace Dom
