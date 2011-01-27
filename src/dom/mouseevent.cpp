#include "pch.h"
#include "mouseevent.h"
#include "../context.h"

namespace Dom {

JSClass MouseEvent::jsClass = {
	"MouseEvent", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

enum PropertyKey {
	screenX, screenY,
	clientX, clientY,
	ctrlKey, shiftKey, altKey, metaKey,
	button
};

static JSBool get(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	MouseEvent* self = reinterpret_cast<MouseEvent*>(JS_GetPrivate(cx, obj));

	switch(JSVAL_TO_INT(id)) {
	case screenX:
		*vp = INT_TO_JSVAL(self->screenX); return JS_TRUE;
	case screenY:
		*vp = INT_TO_JSVAL(self->screenY); return JS_TRUE;
	case clientX:
		*vp = INT_TO_JSVAL(self->clientX); return JS_TRUE;
	case clientY:
		*vp = INT_TO_JSVAL(self->clientY); return JS_TRUE;
	}

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"screenX",		screenX,	0, get, JS_PropertyStub},
	{"screenY",		screenY,	0, get, JS_PropertyStub},
	{"clientX",		clientX,	0, get, JS_PropertyStub},
	{"clientY",		clientY,	0, get, JS_PropertyStub},
	{"ctrlKey",		ctrlKey,	0, get, JS_PropertyStub},
	{"shiftKey",	shiftKey,	0, get, JS_PropertyStub},
	{"altKey",		altKey,		0, get, JS_PropertyStub},
	{"metaKey",		metaKey,	0, get, JS_PropertyStub},
	{"button",		button,		0, get, JS_PropertyStub},
	{0}
};

MouseEvent::MouseEvent()
	: screenX(-1), screenY(-1)
	, clientX(-1), clientY(-1)
	, ctrlKey(false), shiftKey(false)
	, altKey(false), metaKey(false)
	, button(0)
{
}

MouseEvent::~MouseEvent()
{
}

void MouseEvent::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

}	// namespace Dom
