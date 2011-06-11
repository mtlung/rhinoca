#include "pch.h"
#include "windowlocation.h"
#include "window.h"
#include "../context.h"

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	WindowLocation* self = getJsBindable<WindowLocation>(trc->context, obj);
	JS_CALL_OBJECT_TRACER(trc, self->window->jsObject, "WindowLocation::window");
}

JSClass WindowLocation::jsClass = {
	"Location", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
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
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rh->documentUrl.c_str()));
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
	{"href", 0, 0, href, setHref},
	{0}
};

WindowLocation::WindowLocation(Window* w)
	: window(w)
{
}

void WindowLocation::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
