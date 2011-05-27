#include "pch.h"
#include "windowlocation.h"
#include "window.h"
#include "../context.h"

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	WindowLocation* self = reinterpret_cast<WindowLocation*>(JS_GetPrivate(trc->context, obj));
	JS_CallTracer(trc, self->window->jsObject, JSTRACE_OBJECT);
}

JSClass WindowLocation::jsClass = {
	"Location", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
};

static JSBool href(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	WindowLocation* self = reinterpret_cast<WindowLocation*>(JS_GetPrivate(cx, obj));
	Rhinoca* rh = self->window->rhinoca;
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rh->documentUrl.c_str()));
	return JS_TRUE;
}

static JSBool setHref(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	WindowLocation* self = reinterpret_cast<WindowLocation*>(JS_GetPrivate(cx, obj));
	Rhinoca* rh = self->window->rhinoca;
	if(JSString* jss = JS_ValueToString(cx, *vp)) {
		char* str = JS_GetStringBytes(jss);
		rh->openDoucment(str);
		return JS_TRUE;
	}
	return JS_FALSE;
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
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
