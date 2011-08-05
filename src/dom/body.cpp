#include "pch.h"
#include "body.h"
#include "document.h"
#include "../context.h"
#include <string.h>

namespace Dom {

JSClass HTMLBodyElement::jsClass = {
	"HTMLBodyElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static const char* _eventAttributeTable[] = {
	"onload",
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLBodyElement* self = getJsBindable<HTMLBodyElement>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	*vp = self->getEventListenerAsAttribute(cx, _eventAttributeTable[idx]);
	return JS_TRUE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLBodyElement* self = getJsBindable<HTMLBodyElement>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec properties[] = {
	// Event attributes
	{_eventAttributeTable[0], 0, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{0}
};

HTMLBodyElement::HTMLBodyElement(Rhinoca* rh)
	: Element(rh)
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
	addReference();	// releaseReference() in JsBindable::finalize()
}

static const FixString _tagName = "BODY";

Element* HTMLBodyElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, _tagName) == 0 ? new HTMLBodyElement(rh) : NULL;
}

const FixString& HTMLBodyElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
