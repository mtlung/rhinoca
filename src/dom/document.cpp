#include "pch.h"
#include "document.h"
#include "element.h"
#include "node.h"
#include "../common.h"

namespace Dom {

JSClass Document::jsClass = {
	"Document", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool createElement(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
/*	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	Document* doc = reinterpret_cast<Document*>(JS_GetPrivate(cx, obj));
	DocumentElement* ele = doc->createElement(str);
	ele->bindJs(cx, nullptr);
	*rval = OBJECT_TO_JSVAL(ele->jsObject);*/
	return JS_TRUE;
}

static JSBool getElementById(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	Document* doc = reinterpret_cast<Document*>(JS_GetPrivate(cx, obj));
	Element* ele = doc->getElementById(str);

	*rval = ele ? OBJECT_TO_JSVAL(ele->jsObject) : JSVAL_VOID;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"createElement", createElement, 1,0,0},
	{"getElementById", getElementById, 1,0,0},
	{0}
};

Document::Document(Rhinoca* rh)
	: rhinoca(rh)
	, _rootNode(NULL)
{
}

Document::~Document()
{
	_rootNode->removeThis();
}

void Document::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_DefineObject(cx, parent, "document", &jsClass, 0, 0);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));

	addReference();

	_rootNode = new Node;
	_rootNode->bind(jsContext, NULL);
	_rootNode->addGcRoot();
}

Element* Document::getElementById(const char* id)
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

}	// namespace Dom
