#include "pch.h"
#include "Navigator.h"
#include "window.h"
#include "../context.h"

namespace Dom {

JSClass Navigator::jsClass = {
	"Navigator", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods[] = {
	{0}
};

static JSBool userAgent(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Navigator* self = getJsBindable<Navigator>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_InternString(cx, self->userAgent.c_str()));
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"userAgent", 0, JSPROP_READONLY | JsBindable::jsPropFlags, userAgent, JS_StrictPropertyStub},
	{0}
};

Navigator::Navigator()
{
}

void Navigator::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
