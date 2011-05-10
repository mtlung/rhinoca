#include "pch.h"
#include "audio.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"

namespace Dom {

JSClass HTMLAudioElement::jsClass = {
	"HTMLAudioElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec properties[] = {
	{0}
};

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	HTMLAudioElement* audio = new HTMLAudioElement;
	audio->bind(cx, NULL);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	audio->ownerDocument = rh->domWindow->document;

	*rval = OBJECT_TO_JSVAL(audio->jsObject);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLAudioElement::HTMLAudioElement()
{
}

HTMLAudioElement::~HTMLAudioElement()
{
}

void HTMLAudioElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, HTMLMediaElement::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void HTMLAudioElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLAudioElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, "AUDIO") == 0 ? new HTMLAudioElement : NULL;
}

const char* HTMLAudioElement::tagName() const
{
	return "AUDIO";
}

}	// namespace Dom
