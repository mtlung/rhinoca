#include "pch.h"
#include "event.h"

namespace Dom {

JSClass Event::jsClass = {
	"Event", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods[] = {
	{0}
};

static JSBool type(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Event* self = reinterpret_cast<Event*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->type.c_str()));
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"type", 0, JSPROP_READONLY, type, JS_PropertyStub},
	{0}
};

Event::Event()
	: eventPhase(NONE)
	, target(NULL), currentTarget(NULL)
	, bubbles(false), cancelable(false)
{
}

JSObject* Event::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, methods));
	VERIFY(JS_DefineProperties(jsContext, proto, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
	return proto;
}

JsFunctionEventListener::JsFunctionEventListener(JSContext* ctx, jsval closure)
	: _jsContext(ctx), _jsClosure(closure)
{
}

JsFunctionEventListener::~JsFunctionEventListener()
{
}

void JsFunctionEventListener::handleEvent(Event* evt)
{
	jsval argv, rval;
	argv = *evt;
	JS_CallFunctionValue(_jsContext, NULL, _jsClosure, 1, &argv, &rval);
}

void JsFunctionEventListener::jsTrace(JSTracer* trc)
{
	JS_CallTracer(trc, JSVAL_TO_GCTHING(_jsClosure), JSVAL_TRACE_KIND(_jsClosure));
}

EventTarget::EventTarget()
{
}

EventTarget::~EventTarget()
{
}

void EventTarget::addEventListener(const char* type, EventListener* listener, bool useCapture)
{
	ASSERT(listener && !listener->isInList());
	listener->_type = type;
	listener->_useCapture = useCapture;

	// Search for duplication
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		if(l->_type == StringHash(type) && l->_useCapture == useCapture && l->identifier() == listener->identifier()) {
			delete listener;
			return;
		}
	}

	_eventListeners.pushBack(*listener);
}

JSBool EventTarget::addEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture)
{
	JSString* jss = JS_ValueToString(cx, type);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	JSBool capture;
	if(!JS_ValueToBoolean(cx, useCapture, &capture)) return JS_FALSE;

	if(!JS_ValueToFunction(cx, func)) return JS_FALSE;

	JsFunctionEventListener* listener = new JsFunctionEventListener(cx, func);

	addEventListener(str, listener, capture == JS_TRUE);

	return JS_TRUE;
}

void EventTarget::removeEventListener(const char* type, void* listenerIdentifier, bool useCapture)
{
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		if(l->_type == StringHash(type) && l->_useCapture == useCapture && l->identifier() == listenerIdentifier) {
			l->destroyThis();
			return;
		}
	}
}

JSBool EventTarget::removeEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture)
{
	JSString* jss = JS_ValueToString(cx, type);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	JSBool capture;
	if(!JS_ValueToBoolean(cx, useCapture, &capture)) return JS_FALSE;

	if(!JS_ValueToFunction(cx, func)) return JS_FALSE;

	removeEventListener(str, (void*)func, capture == JS_TRUE);

	return JS_TRUE;
}

void EventTarget::removeAllEventListener()
{
	_eventListeners.destroyAll();
}

bool EventTarget::dispatchEvent(Event* evt)
{
	ASSERT(evt);

	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		if(evt->type == l->_type)
			l->handleEvent(evt);
	}

	return true;
}

JSBool EventTarget::dispatchEvent(JSContext* cx, jsval evt)
{
	JSObject* obj = NULL;
	if(JS_ValueToObject(cx, evt, &obj) != JS_TRUE) return JS_FALSE;
	Event* ev = reinterpret_cast<Event*>(JS_GetPrivate(cx, obj));

	dispatchEvent(ev);
	return JS_TRUE;
}

void EventTarget::jsTrace(JSTracer* trc)
{
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next())
		l->jsTrace(trc);
}

}	// namespace Dom
