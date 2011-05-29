#include "pch.h"
#include "keyevent.h"
#include "../context.h"

namespace Dom {

JSClass KeyEvent::jsClass = {
	"KeyEvent", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool keyCode(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	KeyEvent* self = reinterpret_cast<KeyEvent*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->keyCode);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"keyCode", 0, 0, keyCode, JS_PropertyStub},
	{0}
};

KeyEvent::KeyEvent()
	: keyCode(0)
{
}

KeyEvent::~KeyEvent()
{
}

void KeyEvent::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Event::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

}	// namespace Dom
