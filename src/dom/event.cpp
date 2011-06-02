#include "pch.h"
#include "event.h"
#include "../vector.h"

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

JsFunctionEventListener::JsFunctionEventListener(JSContext* ctx)
	: _jsContext(ctx), _jsClosure(0)
	, _jsScript(NULL), _jsScriptObject(NULL)
{
}

JsFunctionEventListener::~JsFunctionEventListener()
{
}

JSBool JsFunctionEventListener::init(jsval stringOrFunc)
{
	JSContext* cx = _jsContext;

	// Compile if the incoming param is string instead of function
	if(JSVAL_IS_STRING(stringOrFunc)) {
		JSString* jss = JS_ValueToString(cx, stringOrFunc);
		_jsScript = JS_CompileScript(cx, JS_GetGlobalObject(cx), JS_GetStringBytes(jss), JS_GetStringLength(jss), NULL, 0);
		if(!_jsScript) return JS_FALSE;
		_jsScriptObject = JS_NewScriptObject(cx, _jsScript);
		if(!_jsScriptObject) { JS_DestroyScript(cx, _jsScript); return JS_FALSE; }
	}
	else if(JS_ValueToFunction(cx, stringOrFunc)) {
		_jsClosure = stringOrFunc;
	}
	else
		return JS_FALSE;

	return JS_TRUE;
}

void JsFunctionEventListener::handleEvent(Event* evt)
{
	jsval argv, rval;
	argv = *evt;
	if(_jsClosure)
		JS_CallFunctionValue(_jsContext, NULL, _jsClosure, 1, &argv, &rval);
	else if(_jsScript)
		JS_ExecuteScript(_jsContext, JS_GetGlobalObject(_jsContext), _jsScript, &rval);
}

void JsFunctionEventListener::jsTrace(JSTracer* trc)
{
	if(_jsClosure)
		JS_CallTracer(trc, JSVAL_TO_GCTHING(_jsClosure), JSVAL_TRACE_KIND(_jsClosure));
	if(_jsScriptObject)
		JS_CallTracer(trc, _jsScriptObject, JSTRACE_OBJECT);
}

ElementAttributeEventListener::ElementAttributeEventListener(JSContext* ctx, const char* eventAttributeName)
	: JsFunctionEventListener(ctx)
	, _eventAttributeName(eventAttributeName)
{
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

	JsFunctionEventListener* listener = new JsFunctionEventListener(cx);
	
	if(listener->init(func))
		addEventListener(str, listener, capture == JS_TRUE);
	else {
		delete listener;
		return JS_FALSE;
	}

	return JS_TRUE;
}

JSBool EventTarget::addEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName, jsval stringOrFunc)
{
	if(JSVAL_IS_NULL(stringOrFunc) || JSVAL_IS_VOID(stringOrFunc))
		return JS_TRUE;

	ElementAttributeEventListener* listener = new ElementAttributeEventListener(cx, eventAttributeName);

	if(listener->init(stringOrFunc)) {
		// Skip the 'on' of the event attribute will become the eventType
		const char* eventType = eventAttributeName + 2;
		addEventListener(eventType, listener, true);
	}
	else {
		delete listener;
		return JS_FALSE;
	}

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

JSBool EventTarget::removeEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName)
{
	removeEventListener(eventAttributeName + 2, (void*)FixString(eventAttributeName).hashValue(), true);
	return JS_TRUE;
}

void EventTarget::removeAllEventListener()
{
	_eventListeners.destroyAll();
}

bool EventTarget::_dispatchEventNoCaptureBubble(Event* evt)
{
	ASSERT(evt);

	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		// Check for capture/bubble phase
		bool correctPhase = evt->eventPhase == Event::AT_TARGET;
		correctPhase |= l->_useCapture == (evt->eventPhase == Event::CAPTURING_PHASE);

		// Check for correct event type
		if(correctPhase && evt->type == l->_type)
			l->handleEvent(evt);
	}

	return true;
}

JSBool EventTarget::dispatchEvent(Event* ev)
{
	// Build the event propagation list
	// See http://docstore.mik.ua/orelly/webprog/dhtml/ch06_05.htm
	Vector<EventTarget*> list(1, this);

	for(EventTarget* t = this->eventTargetTraverseUp(); t; t = t->eventTargetTraverseUp()) {
		if(!t->_eventListeners.isEmpty())
			list.push_back(t);
	}

	// Add reference to all EventTarget before doing any callback 
	for(unsigned i=0; i<list.size(); ++i)
		list[i]->eventTargetAddReference();

	// Perform capture phase (traverse down), including target
	for(unsigned i=list.size(); i--; ) {
		ev->eventPhase = list[i] == this ? Event::AT_TARGET : Event::CAPTURING_PHASE;
		list[i]->_dispatchEventNoCaptureBubble(ev);
	}

	// TODO: Should the bubble phase use the most updated tree structure?
	// as it might changed it the capture phase?

	// Perform bubble phase (traverse up), excluding target
	for(unsigned i=1; i<list.size(); ++i) {
		ev->eventPhase = list[i] == this ? Event::AT_TARGET : Event::BUBBLING_PHASE;
		list[i]->_dispatchEventNoCaptureBubble(ev);
	}

	// Now we can release the reference
	for(unsigned i=0; i<list.size(); ++i)
		list[i]->eventTargetReleaseReference();

	return JS_TRUE;
}

void EventTarget::jsTrace(JSTracer* trc)
{
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next())
		l->jsTrace(trc);
}

}	// namespace Dom
