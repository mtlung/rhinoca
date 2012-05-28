#include "pch.h"
#include "touchevent.h"
#include "../context.h"

namespace Dom {

// class Touch

JSClass Touch::jsClass = {
	"Touch", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

enum touch_PropertyKey {
	identifier,
	screenX, screenY,
	clientX, clientY,
	pageX, pageY,
};

static JSBool touch_get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Touch* self = getJsBindable<Touch>(cx, obj);

	switch(JSID_TO_INT(id)) {
	case identifier:
		*vp = INT_TO_JSVAL(self->data.identifier); return JS_TRUE;
	case screenX:
		*vp = INT_TO_JSVAL(self->data.screenX); return JS_TRUE;
	case screenY:
		*vp = INT_TO_JSVAL(self->data.screenY); return JS_TRUE;
	case clientX:
		*vp = INT_TO_JSVAL(self->data.clientX); return JS_TRUE;
	case clientY:
		*vp = INT_TO_JSVAL(self->data.clientY); return JS_TRUE;
	case pageX:
		*vp = INT_TO_JSVAL(self->data.pageX); return JS_TRUE;
	case pageY:
		*vp = INT_TO_JSVAL(self->data.pageY); return JS_TRUE;
	}

	return JS_TRUE;
}

static JSPropertySpec touch_properties[] = {
	{"screenX",		screenX,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	// Returns coordinates relative to the screen
	{"screenY",		screenY,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	//
	{"clientX",		clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	// Returns coordinates relative to the window
	{"clientY",		clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	//
	{"x",			clientX,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	//
	{"y",			clientY,	JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	//
	{"pageX",		pageX,		JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	// Returns coordinates relative to the document
	{"pageY",		pageY,		JSPROP_READONLY | JsBindable::jsPropFlags, touch_get, JS_StrictPropertyStub},	//
	{0}
};

void Touch::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineProperties(cx, *this, touch_properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

// class TouchList

JSClass TouchList::jsClass = {
	"TouchList", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool item(JSContext* cx, uintN argc, jsval* vp)
{
	TouchList* self = getJsBindable<TouchList>(cx, vp);

	int32 index;
	if(!JS_ValueToInt32(cx, JS_ARGV0, &index)) return JS_FALSE;

	if(index < 0 || index >= (int)self->touches.size()) {
		JS_RVAL(cx, vp) = JSVAL_NULL;
		return JS_TRUE;
	}

	Touch* touch = new Touch;
	touch->data = self->touches[index];
	touch->bind(cx, *self);

	JS_RVAL(cx, vp) = *touch;
	return JS_TRUE;
}

static JSBool identifiedTouch(JSContext* cx, uintN argc, jsval* vp)
{
	TouchList* self = getJsBindable<TouchList>(cx, vp);

	int32 identifier;
	if(!JS_ValueToInt32(cx, JS_ARGV0, &identifier)) return JS_FALSE;

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	ro::Array<TouchData>& touches = rh->domWindow->touches;

	// Scan touch with the same identifier
	int32 index = rh->domWindow->findTouchIndexByIdentifier(identifier);
	roAssert(index < (int)touches.size());

	if(index < 0) {
		JS_RVAL(cx, vp) = JSVAL_NULL;
		return JS_TRUE;
	}

	Touch* touch = new Touch;
	touch->data = touches[index];
	touch->bind(cx, *self);

	JS_RVAL(cx, vp) = *touch;
	return JS_TRUE;
}

static JSFunctionSpec touchList_methods[] = {
	{"item", item, 1,0},
	{"identifiedTouch", identifiedTouch, 1,0},
	{0}
};

static JSBool length(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	TouchList* self = getJsBindable<TouchList>(cx, obj);
	*vp = INT_TO_JSVAL(self->touches.size());
	return JS_TRUE;
}

static JSPropertySpec touchList_properties[] = {
	{"length", 0, JSPROP_READONLY | JsBindable::jsPropFlags, length, JS_StrictPropertyStub},
	{0}
};

void TouchList::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, touchList_methods));
	roVerify(JS_DefineProperties(cx, *this, touchList_properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

// class ToucchEvent

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
	// Arg: ctrlKey
	// Arg: altKey
	// Arg: shiftKey
	// Arg: metaKey

	// Arg: touches
	if(TouchList* tl = getJsBindable<TouchList>(cx, vp, 9))
		self->touches = tl->touches;
	else
		return JS_FALSE;

	// Arg: targetTouches
	if(TouchList* tl = getJsBindable<TouchList>(cx, vp, 10))
		self->targetTouches = tl->touches;
	else
		return JS_FALSE;

	// Arg: changedTouches
	if(TouchList* tl = getJsBindable<TouchList>(cx, vp, 11))
		self->changedTouches = tl->touches;
	else
		return JS_FALSE;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"initTouchEvent", initTouchEvent, 15,0},	// https://developer.mozilla.org/en/DOM/event.initTouchEvent
	{0}
};

enum PropertyKey {
	touches,
	targetTouches,
	changedTouches
};

static JSBool target(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	TouchEvent* self = getJsBindable<TouchEvent>(cx, obj);
	*vp = OBJECT_TO_JSVAL(self->target->getJSObject());
	return JS_TRUE;
}

static JSBool getTouches(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	TouchEvent* self = getJsBindable<TouchEvent>(cx, obj);

	TouchList* list = new TouchList;
	list->bind(cx, *self);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	ro::Array<TouchData>& _touches = rh->domWindow->touches;

	switch(JSID_TO_INT(id)) {
	case touches:
		list->touches = self->touches;
		break;
	case targetTouches:
		list->touches = self->targetTouches;
		break;
	case changedTouches:
		list->touches = self->changedTouches;
		break;
	default: roAssert(false);
	}

	*vp = *list;

	return JS_TRUE;
}

// Reference:
// http://www.quirksmode.org/dom/w3c_cssom.html#mousepos
static JSPropertySpec properties[] = {
	{"target",			0,				JSPROP_READONLY | JsBindable::jsPropFlags, target, JS_StrictPropertyStub},
	{"touches",			touches,		JSPROP_READONLY | JsBindable::jsPropFlags, getTouches, JS_StrictPropertyStub},
	{"targetTouches",	targetTouches,	JSPROP_READONLY | JsBindable::jsPropFlags, getTouches, JS_StrictPropertyStub},
	{"changedTouches",	changedTouches,	JSPROP_READONLY | JsBindable::jsPropFlags, getTouches, JS_StrictPropertyStub},
	{0}
};

TouchEvent::TouchEvent()
{
}

TouchEvent::~TouchEvent()
{
}

void TouchEvent::bind(JSContext* cx, JSObject* parent)
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
