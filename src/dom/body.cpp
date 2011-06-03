#include "pch.h"
#include "body.h"
#include "document.h"
#include "../array.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLBodyElement::jsClass = {
	"HTMLBodyElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

const Array<const char*, 3> _eventAttributeTable = {
	"onload",
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	// Not implemented
	return JS_FALSE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLBodyElement* self = reinterpret_cast<HTMLBodyElement*>(JS_GetPrivate(cx, obj));
	id /= 2 + 0;	// Account for having both get and set functions

	// NOTE: Redirect body.onload to window.onload
	return self->ownerDocument->window()->addEventListenerAsAttribute(cx, _eventAttributeTable[id], *vp);
}

static JSPropertySpec properties[] = {
	// Event attributes
	{_eventAttributeTable[0], 0, 0, getEventAttribute, setEventAttribute},
	{0}
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
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

Element* HTMLBodyElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, "BODY") == 0 ? new HTMLBodyElement : NULL;
}

static const FixString _tagName = "BODY";

const FixString& HTMLBodyElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
