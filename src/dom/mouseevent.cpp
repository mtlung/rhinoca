#include "pch.h"
#include "mouseevent.h"
#include "../context.h"

namespace Dom {

JSClass MouseEvent::jsClass = {
	"MouseEvent", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool initMouseEvent(JSContext* cx, uintN argc, jsval* vp)
{
	MouseEvent* self = getJsBindable<MouseEvent>(cx, vp);

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

	// Arg: ctrlKey
	if(JS_GetValue(cx, JS_ARGV9, self->ctrlKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: altKey
	if(JS_GetValue(cx, JS_ARGV(cx, vp)[10], self->altKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: shiftKey
	if(JS_GetValue(cx, JS_ARGV(cx, vp)[11], self->shiftKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: metaKey
	if(JS_GetValue(cx, JS_ARGV(cx, vp)[12], self->metaKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: button
	if(JS_GetValue(cx, JS_ARGV(cx, vp)[13], self->button) == JS_FALSE)
		return JS_FALSE;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"initMouseEvent", initMouseEvent, 15,0},	// https://developer.mozilla.org/en/DOM/event.initMouseEvent
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
	MouseEvent* self = getJsBindable<MouseEvent>(cx, obj);

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
	{"screenX",		screenX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns the mouse coordinates relative to the screen
	{"screenY",		screenY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"clientX",		clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns the mouse coordinates relative to the window
	{"clientY",		clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"x",			clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"y",			clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"pageX",		pageX,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	// Returns the mouse coordinates relative to the document
	{"pageY",		pageY,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},	//
	{"ctrlKey",		ctrlKey,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"shiftKey",	shiftKey,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"altKey",		altKey,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"metaKey",		metaKey,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"button",		button,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},

	{"target",		target,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{0}
};

MouseEvent::MouseEvent()
	: screenX(-1), screenY(-1)
	, clientX(-1), clientY(-1)
	, pageX(-1), pageY(-1)
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
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Event::createPrototype(), parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
