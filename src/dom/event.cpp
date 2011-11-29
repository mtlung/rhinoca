#include "pch.h"
#include "event.h"
#include "../context.h"
#include "../../roar/base/roStringHash.h"

using namespace ro;

namespace Dom {

JSClass Event::jsClass = {
	"Event", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool initEvent(JSContext* cx, uintN argc, jsval* vp)
{
	Event* self = getJsBindable<Event>(cx, vp);

	// Arg: type
	if(JS_GetValue(cx, JS_ARGV0, self->type) == JS_FALSE)
		return JS_FALSE;

	// Arg: canBubble
	if(JS_GetValue(cx, JS_ARGV1, self->bubbles) == JS_FALSE)
		return JS_FALSE;

	// TODO:
	// Arg: cancelable 

	return JS_TRUE;
}

static JSBool preventDefault(JSContext* cx, uintN argc, jsval* vp)
{
	// Currently just do nothing for preventDefault()
	return JS_TRUE;
}

static JSBool stopPropagation(JSContext* cx, uintN argc, jsval* vp)
{
	Event* self = getJsBindable<Event>(cx, vp);
	self->stopPropagation();
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"initEvent", initEvent, 3,0},
	{"preventDefault", preventDefault, 0,0},
	{"stopPropagation", stopPropagation, 0,0},
	{0}
};

static JSBool type(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Event* self = getJsBindable<Event>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->type));
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"type", 0, JSPROP_READONLY | JsBindable::jsPropFlags, type, JS_StrictPropertyStub},
	{0}
};

Event::Event()
	: eventPhase(NONE)
	, target(NULL), currentTarget(NULL)
	, bubbles(false), cancelable(false)
	, _stopPropagation(false)
{
}

void Event::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

JSObject* Event::createPrototype()
{
	RHASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, NULL, NULL);
	RHVERIFY(JS_SetPrivate(jsContext, proto, this));
	RHVERIFY(JS_DefineFunctions(jsContext, proto, methods));
	RHVERIFY(JS_DefineProperties(jsContext, proto, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
	return proto;
}

void Event::stopPropagation()
{
	_stopPropagation = true;
}

JsFunctionEventListener::JsFunctionEventListener(JSContext* ctx)
	: _jsContext(ctx), _jsClosure(JSVAL_NULL)
	, _jsScript(NULL)
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
		JsString s(cx, stringOrFunc);
		_jsScript = JS_CompileScript(cx, JS_GetGlobalObject(cx), s.c_str(), s.size(), NULL, 0);
		if(!_jsScript) return JS_FALSE;
	}
	else if(JS_ValueToFunction(cx, stringOrFunc)) {
		_jsClosure = stringOrFunc;
	}
	else
		return JS_FALSE;

	return JS_TRUE;
}

void JsFunctionEventListener::handleEvent(Event* evt, JSObject* self)
{
	jsval argv, rval;
	argv = *evt;

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(_jsContext));
	rh->domWindow->currentEvent = evt;

	if(!JSVAL_IS_NULL(_jsClosure))
		JS_CallFunctionValue(_jsContext, self, _jsClosure, 1, &argv, &rval);
	else if(_jsScript)
		JS_ExecuteScript(_jsContext, self, _jsScript, &rval);

	rh->domWindow->currentEvent = NULL;
}

jsval JsFunctionEventListener::getJsVal()
{
	if(_jsScript)
		return OBJECT_TO_JSVAL(_jsScript);
	else
		return _jsClosure;
}

void JsFunctionEventListener::jsTrace(JSTracer* trc)
{
	if(!JSVAL_IS_NULL(_jsClosure))
		JS_CALL_VALUE_TRACER(trc, _jsClosure, "JsFunctionEventListener._jsClosure");
	if(_jsScript)
		JS_CALL_OBJECT_TRACER(trc, _jsScript, "JsFunctionEventListener._jsScript");
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
	RHASSERT(listener && !listener->isInList());
	listener->_type = type;
	listener->_useCapture = useCapture;

	// Search for duplication and remove it before inserting new one
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		if(l->_type == stringHash(type, 0) && l->_useCapture == useCapture && l->identifier() == listener->identifier()) {
			delete l;
			break;
		}
	}

	_eventListeners.pushBack(*listener);
}

JSBool EventTarget::addEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture)
{
	JsString jss(cx, type);
	if(!jss) return JS_FALSE;

	JSBool capture;
	if(!JS_ValueToBoolean(cx, useCapture, &capture)) return JS_FALSE;

	JsFunctionEventListener* listener = new JsFunctionEventListener(cx);
	
	if(listener->init(func))
		addEventListener(jss.c_str(), listener, capture == JS_TRUE);
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

		// Remove any existing event attribute
		// NOTE: Event attribute use bubbling by default
		addEventListener(eventType, listener, false);
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
		if(l->_type == stringHash(type, 0) && l->_useCapture == useCapture && l->identifier() == listenerIdentifier) {
			l->destroyThis();
			return;
		}
	}
}

JSBool EventTarget::removeEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture)
{
	JsString jss(cx, type);
	if(!jss) return JS_FALSE;

	JSBool capture;
	if(!JS_ValueToBoolean(cx, useCapture, &capture)) return JS_FALSE;

	if(!JS_ValueToFunction(cx, func)) return JS_FALSE;

	removeEventListener(jss.c_str(), JSVAL_TO_OBJECT(func), capture == JS_TRUE);

	return JS_TRUE;
}

JSBool EventTarget::removeEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName)
{
	removeEventListener(eventAttributeName + 2, (void*)ro::ConstString(eventAttributeName).hash(), false);
	return JS_TRUE;
}

jsval EventTarget::getEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName)
{
	void* listenerIdentifier = (void*)ro::ConstString(eventAttributeName).hash();

	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		if(l->_type == stringHash(eventAttributeName + 2, 0) && !l->_useCapture && l->identifier() == listenerIdentifier) {
			if(JsFunctionEventListener* e = dynamic_cast<JsFunctionEventListener*>(l))
				return e->getJsVal();
		}
	}

	return JSVAL_NULL;
}

void EventTarget::removeAllEventListener()
{
	_eventListeners.destroyAll();
}

bool EventTarget::_dispatchEventNoCaptureBubble(Event* evt, JSObject* self)
{
	RHASSERT(evt);

	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); ) {
		// NOTE: In case the event listener callback invoke removeEventListener()
		EventListener* next = l->next();

		// Check for capture/bubble phase
		bool correctPhase = evt->eventPhase == Event::AT_TARGET;
		correctPhase |= l->_useCapture == (evt->eventPhase == Event::CAPTURING_PHASE);

		// Check for correct event type
		if(correctPhase && evt->type == l->_type)
			l->handleEvent(evt, self ? self : getJSObject());

		l = next;
	}

	return true;
}

JSBool EventTarget::dispatchEvent(Event* ev, JSObject* self)
{
	// Build the event propagation list
	// See http://docstore.mik.ua/orelly/webprog/dhtml/ch06_05.htm
	TinyArray<EventTarget*, 64> list(1, this);

	for(EventTarget* t = this->eventTargetTraverseUp(); t; t = t->eventTargetTraverseUp()) {
		if(!t->_eventListeners.isEmpty())
			list.pushBack(t);
	}

	// Add reference to all EventTarget before doing any callback 
	for(unsigned i=0; i<list.size(); ++i)
		list[i]->eventTargetAddReference();

	// Perform capture phase (traverse down), including target
	for(unsigned i=list.size(); i--; ) {
		ev->eventPhase = list[i] == this ? Event::AT_TARGET : Event::CAPTURING_PHASE;
		list[i]->_dispatchEventNoCaptureBubble(ev, self);
	}

	// TODO: Should the bubble phase use the most updated tree structure?
	// as it might changed it the capture phase?

	// Perform bubble phase (traverse up), excluding target
	for(unsigned i=1; i<list.size() && !ev->_stopPropagation; ++i) {
		ev->eventPhase = list[i] == this ? Event::AT_TARGET : Event::BUBBLING_PHASE;
		list[i]->_dispatchEventNoCaptureBubble(ev, self);
	}

	// Now we can release the reference
	for(unsigned i=0; i<list.size(); ++i)
		list[i]->eventTargetReleaseReference();

	return JS_TRUE;
}

void EventTarget::jsTrace(JSTracer* trc)
{
	for(EventListener* l = _eventListeners.begin(); l != _eventListeners.end(); l = l->next()) {
		l->jsTrace(trc);
	}
}

}	// namespace Dom
