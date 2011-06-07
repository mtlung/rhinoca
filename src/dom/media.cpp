#include "pch.h"
#include "media.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../xmlparser.h"
#include <string.h>	// for strcasecmp

namespace Dom {

JSClass HTMLMediaElement::jsClass = {
	"HTMLMediaElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->src()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	Path path;
	self->fixRelativePath(str, self->ownerDocument->rhinoca->documentUrl.c_str(), path);
	self->setSrc(path.c_str());

	return JS_TRUE;
}

static JSBool getReadyState(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->readyState); return JS_TRUE;
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

static JSBool getCurrentTime(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	*vp = DOUBLE_TO_JSVAL(JS_NewDouble(cx, self->currentTime())); return JS_TRUE;
}

static JSBool setCurrentTime(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	double a;
	if(JS_ValueToNumber(cx, *vp, &a) == JS_FALSE)
		return JS_FALSE;
	self->setCurrentTime(a); return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"src", 0, 0, getSrc, setSrc},
	{"autoplay", 0, 0, getAutoplay, setAutoplay},
	{"currentTime", 0, 0, getCurrentTime, setCurrentTime},
	{0}
};

static JSBool play(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	self->play();
	return JS_TRUE;
}

static JSBool pause(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));
	self->pause();
	return JS_TRUE;
}

static JSBool canPlayType(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLMediaElement* self = reinterpret_cast<HTMLMediaElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, argv[0]);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	if( strcasecmp(str, "audio/mpeg") == 0 ||
		strcasecmp(str, "audio/ogg") == 0 ||
		strcasecmp(str, "audio/x-wav") == 0)
	{
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "probably"));
	}
	else
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"play", play, 0,0,0},
	{"pause", pause, 0,0,0},
	{"canPlayType", canPlayType, 1,0,0},
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
	addReference();	// releaseReference() in JsBindable::finalize()
	return proto;
}

void HTMLMediaElement::parseMediaElementAttributes(Rhinoca* rh, XmlParser* parser)
{
	if(const char* s = parser->attributeValueIgnoreCase("src")) {
		Path path;
		fixRelativePath(s, rh->documentUrl.c_str(), path);
		setSrc(path.c_str());
	}
}

}	// namespace Dom
