#include "pch.h"
#include "div.h"
#include "document.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLDivElement::jsClass = {
	"HTMLDivElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

HTMLDivElement::HTMLDivElement()
{
}

HTMLDivElement::~HTMLDivElement()
{
}

void HTMLDivElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
//	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

Element* HTMLDivElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, "DIV") == 0 ? new HTMLDivElement : NULL;
}

const char* HTMLDivElement::tagName() const
{
	return "DIV";
}

}	// namespace Dom
