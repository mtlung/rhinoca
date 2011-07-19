#include "pch.h"
#include "touchevent.h"
#include "../context.h"

namespace Dom {

JSClass TouchEvent::jsClass = {
	"TouchEvent", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool initTouchEvent(JSContext* cx, uintN argc, jsval* vp)
{
	TouchEvent* self = getJsBindable<TouchEvent>(cx, vp);

	// Arg: type
	if(JS_GetValue(cx, JS_ARGV0, self->type) == JS_FALSE)
		return JS_FALSE;

	// Arg: canBubble
	if(JS_GetValue(cx, JS_ARGV1, self->bubbles) == JS_FALSE)
		return JS_FALSE;

	// TODO:
	// Arg: cancelable 
	// Arg: view
	// Arg: detail

	// Arg: screenX
	if(JS_GetValue(cx, JS_ARGV5, self->screenX) == JS_FALSE)
		return JS_FALSE;

	// Arg: screenX
	if(JS_GetValue(cx, JS_ARGV6, self->screenY) == JS_FALSE)
		return JS_FALSE;

	// Arg: clientX
	if(JS_GetValue(cx, JS_ARGV7, self->clientX) == JS_FALSE)
		return JS_FALSE;

	// Arg: clientY
	if(JS_GetValue(cx, JS_ARGV8, self->clientY) == JS_FALSE)
		return JS_FALSE;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"initTouchEvent", initTouchEvent, 15,0},	// https://developer.mozilla.org/en/DOM/event.initTouchEvent
	{0}
};

enum PropertyKey {
	screenX, screenY,
	clientX, clientY,
	pageX, pageY,
	ctrlKey, shiftKey, altKey, metaKey,
	button,
	target
};

static JSBool get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	TouchEvent* self = getJsBindable<TouchEvent>(cx, obj);

	switch(JSID_TO_INT(id)) {
	case screenX:
		*vp = INT_TO_JSVAL(self->screenX); return JS_TRUE;
	case screenY:
		*vp = INT_TO_JSVAL(self->screenY); return JS_TRUE;
	case clientX:
		*vp = INT_TO_JSVAL(self->clientX); return JS_TRUE;
	case clientY:
		*vp = INT_TO_JSVAL(self->clientY); return JS_TRUE;
	case pageX:
		*vp = INT_TO_JSVAL(self->pageX); return JS_TRUE;
	case pageY:
		*vp = INT_TO_JSVAL(self->pageY); return JS_TRUE;
	case target:
		*vp = OBJECT_TO_JSVAL(self->target->getJSObject()); return JS_TRUE;
	}

	return JS_TRUE;
}

// Reference:
// http://www.quirksmode.org/dom/w3c_cssom.html#mousepos
static JSPropertySpec properties[] = {
	{"screenX",		screenX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns coordinates relative to the screen
	{"screenY",		screenY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"clientX",		clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns coordinates relative to the window
	{"clientY",		clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"x",			clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"y",			clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"pageX",		pageX,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns coordinates relative to the document
	{"pageY",		pageY,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//

	{"target",		target,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{0}
};

TouchEvent::TouchEvent()
	: screenX(-1), screenY(-1)
	, clientX(-1), clientY(-1)
	, pageX(-1), pageY(-1)
{
}

TouchEvent::~TouchEvent()
{
}

void TouchEvent::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Event::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

}	// namespace Dom
