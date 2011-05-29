#include "pch.h"
#include "mouseevent.h"
#include "../context.h"

namespace Dom {

static JSBool JS_GetValue(JSContext *cx, jsval jv, bool& val)
{
	if(JS_ValueToBoolean(cx, jv, (JSBool*)&val) == JS_FALSE)
		return JS_FALSE;
	return JS_TRUE;
}

static JSBool JS_GetValue(JSContext *cx, jsval jv, int& val)
{
	int32 i;
	if(JS_ValueToInt32(cx, jv, &i) == JS_FALSE)
		return JS_FALSE;
	val = i;
	return JS_TRUE;
}

static JSBool JS_GetValue(JSContext *cx, jsval jv, unsigned& val)
{
	uint32 i;
	if(JS_ValueToECMAUint32(cx, jv, &i) == JS_FALSE)
		return JS_FALSE;
	val = i;
	return JS_TRUE;
}

static JSBool JS_GetValue(JSContext *cx, jsval jv, FixString& val)
{
	if(JSString* jss = JS_ValueToString(cx, jv)) {
		val = JS_GetStringBytes(jss);
		return JS_TRUE;
	}
	return JS_FALSE;
}

JSClass MouseEvent::jsClass = {
	"MouseEvent", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool initMouseEvent(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	MouseEvent* self = reinterpret_cast<MouseEvent*>(JS_GetPrivate(cx, obj));

	// Arg: type
	if(JS_GetValue(cx, argv[0], self->type) == JS_FALSE)
		return JS_FALSE;

	// Arg: canBubble
	if(JS_GetValue(cx, argv[1], self->bubbles) == JS_FALSE)
		return JS_FALSE;

	// TODO:
	// Arg: cancelable 
	// Arg: view
	// Arg: detail

	// Arg: screenX
	if(JS_GetValue(cx, argv[5], self->screenX) == JS_FALSE)
		return JS_FALSE;

	// Arg: screenX
	if(JS_GetValue(cx, argv[6], self->screenY) == JS_FALSE)
		return JS_FALSE;

	// Arg: clientX
	if(JS_GetValue(cx, argv[7], self->clientX) == JS_FALSE)
		return JS_FALSE;

	// Arg: clientY
	if(JS_GetValue(cx, argv[8], self->clientY) == JS_FALSE)
		return JS_FALSE;

	// Arg: ctrlKey
	if(JS_GetValue(cx, argv[9], self->ctrlKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: altKey
	if(JS_GetValue(cx, argv[10], self->altKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: shiftKey
	if(JS_GetValue(cx, argv[11], self->shiftKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: metaKey
	if(JS_GetValue(cx, argv[12], self->metaKey) == JS_FALSE)
		return JS_FALSE;

	// Arg: button
	if(JS_GetValue(cx, argv[13], self->button) == JS_FALSE)
		return JS_FALSE;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"initMouseEvent", initMouseEvent, 15,0,0},	// https://developer.mozilla.org/en/DOM/event.initMouseEvent
	{0}
};

enum PropertyKey {
	screenX, screenY,
	clientX, clientY,
	pageX, pageY,
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
	case pageX:
		*vp = INT_TO_JSVAL(self->pageX); return JS_TRUE;
	case pageY:
		*vp = INT_TO_JSVAL(self->pageY); return JS_TRUE;
	}

	return JS_TRUE;
}

// Reference:
// http://www.quirksmode.org/dom/w3c_cssom.html#mousepos
static JSPropertySpec properties[] = {
	{"screenX",		screenX,	0, get, JS_PropertyStub},	// Returns the mouse coordinates relative to the screen
	{"screenY",		screenY,	0, get, JS_PropertyStub},	//
	{"clientX",		clientX,	0, get, JS_PropertyStub},	// Returns the mouse coordinates relative to the window
	{"clientY",		clientY,	0, get, JS_PropertyStub},	//
	{"x",			clientX,	0, get, JS_PropertyStub},	//
	{"y",			clientY,	0, get, JS_PropertyStub},	//
	{"pageX",		pageX,		0, get, JS_PropertyStub},	// Returns the mouse coordinates relative to the document
	{"pageY",		pageY,		0, get, JS_PropertyStub},	//
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
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Event::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

}	// namespace Dom
