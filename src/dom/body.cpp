#include "pch.h"
#include "div.h"
#include "body.h"
#include "../context.h"

namespace Dom {

JSClass HTMLBodyElement::jsClass = {
	"HTMLBodyElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

HTMLBodyElement::HTMLBodyElement()
{
}

HTMLBodyElement::~HTMLBodyElement()
{
}

void HTMLBodyElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
//	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
//	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

Element* HTMLBodyElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, "BODY") == 0 ? new HTMLBodyElement : NULL;
}

const char* HTMLBodyElement::tagName() const
{
	return "BODY";
}

}	// namespace Dom