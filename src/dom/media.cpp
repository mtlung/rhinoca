#include "pch.h"
#include "media.h"
#include "document.h"
#include "../context.h"
#include "../path.h"

namespace Dom {

JSClass HTMLMediaElement::jsClass = {
	"HTMLMediaElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
//	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
//	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->texture->uri().c_str()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	Path path;
	if(Path(str).hasRootDirectory())	// Absolute path
		path = str;
	else {
		// Relative path to the document
		path = self->ownerDocument->rhinoca->documentUrl.c_str();
		path = path.getBranchPath() / str;
	}

	// TODO:

	return JS_TRUE;
}

static JSBool getReadyState(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->readyState()); return JS_TRUE;
}

static JSBool getAutoplay(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	*vp = BOOLEAN_TO_JSVAL(self->autoplay()); return JS_TRUE;
}

static JSBool setAutoplay(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	self->setAutoplay(JSVAL_TO_BOOLEAN(*vp) == JS_TRUE); return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"src", 0, 0, getSrc, setSrc},
	{"autoplay", 0, 0, getAutoplay, setAutoplay},
	{0}
};

static JSFunctionSpec methods[] = {
	{0}
};

HTMLMediaElement::HTMLMediaElement()
{
}

HTMLMediaElement::~HTMLMediaElement()
{
}

JSObject* HTMLMediaElement::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, Element::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, methods));
	VERIFY(JS_DefineProperties(jsContext, proto, properties));
	addReference();
	return proto;
}

}	// namespace Dom
