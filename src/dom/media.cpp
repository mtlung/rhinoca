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
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getSrc(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->src()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	Path path;
	self->fixRelativePath(jss.c_str(), self->ownerDocument->rhinoca->documentUrl.c_str(), path);
	self->setSrc(path.c_str());

	return JS_TRUE;
}

static JSBool getReadyState(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->readyState); return JS_TRUE;
}

static JSBool getAutoplay(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
	*vp = BOOLEAN_TO_JSVAL(self->autoplay()); return JS_TRUE;
}

static JSBool setAutoplay(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
	self->setAutoplay(JSVAL_TO_BOOLEAN(*vp) == JS_TRUE); return JS_TRUE;
}

static JSBool getCurrentTime(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
	*vp = DOUBLE_TO_JSVAL(self->currentTime()); return JS_TRUE;
}

static JSBool setCurrentTime(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, obj);
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

static JSBool play(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, vp);
	self->play();
	return JS_TRUE;
}

static JSBool pause(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, vp);
	self->pause();
	return JS_TRUE;
}

static JSBool canPlayType(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLMediaElement* self = getJsBindable<HTMLMediaElement>(cx, vp);

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	if( strcasecmp(jss.c_str(), "audio/mpeg") == 0 ||
		strcasecmp(jss.c_str(), "audio/ogg") == 0 ||
		strcasecmp(jss.c_str(), "audio/x-wav") == 0)
	{
		JS_RVAL(cx, vp) = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "probably"));
	}
	else
		JS_RVAL(cx, vp) = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"play", play, 0,0},
	{"pause", pause, 0,0},
	{"canPlayType", canPlayType, 1,0},
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
