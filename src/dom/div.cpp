#include "pch.h"
#include "div.h"
#include "document.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLDivElement::jsClass = {
	"HTMLDivElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

HTMLDivElement::HTMLDivElement(Rhinoca* rh)
	: Element(rh)
{
}

HTMLDivElement::~HTMLDivElement()
{
}

void HTMLDivElement::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
//	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
//	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

static const ro::ConstString _tagName = "DIV";

Element* HTMLDivElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, _tagName) == 0 ? new HTMLDivElement(rh) : NULL;
}

const ro::ConstString& HTMLDivElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
