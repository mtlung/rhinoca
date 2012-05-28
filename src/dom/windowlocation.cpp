#include "pch.h"
#include "windowlocation.h"
#include "window.h"
#include "../context.h"

namespace Dom {

JSClass WindowLocation::jsClass = {
	"Location", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool assign(JSContext* cx, uintN argc, jsval* vp)
{
	WindowLocation* self = getJsBindable<WindowLocation>(cx, vp);
	Rhinoca* rh = self->window->rhinoca;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	rh->openDoucment(jss.c_str());
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"assign", assign, 1,0},
	{"replace", assign, 1,0},
	{0}
};

static JSBool href(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	WindowLocation* self = getJsBindable<WindowLocation>(cx, obj);
	Rhinoca* rh = self->window->rhinoca;
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rh->documentUrl));
	return JS_TRUE;
}

static JSBool setHref(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	WindowLocation* self = getJsBindable<WindowLocation>(cx, obj);
	Rhinoca* rh = self->window->rhinoca;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	rh->openDoucment(jss.c_str());
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"href", 0, JsBindable::jsPropFlags, href, setHref},
	{0}
};

WindowLocation::WindowLocation(Window* w)
	: window(w)
{
}

void WindowLocation::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()

	roVerify(JS_SetReservedSlot(cx, jsObject, 0, *window));
}

}	// namespace Dom
