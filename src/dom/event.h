#ifndef __DOM_EVENT_H__
#define __DOM_EVENT_H__

#include "../jsbindable.h"
#include "../../roar/base/roLinkList.h"

// Documentation for DOM event model:
// http://www.w3.org/TR/DOM-Level-2-Events/events.html
// Good explanation on event:
// http://www.howtocreate.co.uk/tutorials/javascript/domevents

namespace Dom {

class EventTarget;

class Event : public JsBindable
{
public:
	Event();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;
	JSObject* createPrototype();

	/// Stop the event from bubbling up
	void stopPropagation();

// Attributes
	enum Phase {
		NONE = 0,
		CAPTURING_PHASE = 1,
		AT_TARGET = 2,
		BUBBLING_PHASE = 3
	};

	ro::ConstString type;
	Phase eventPhase;
	EventTarget* target;
	EventTarget* currentTarget;
	bool bubbles;
	bool cancelable;

	static JSClass jsClass;

protected:
	friend class EventTarget;
	bool _stopPropagation;
};	// Event

class EventListener : public ro::ListNode<EventListener>
{
public:
	virtual void handleEvent(Event* evt, JSObject* self) = 0;

protected:
	friend class EventTarget;

	/// The signature of the EventListener
	virtual void* identifier() { return this; }
	virtual void jsTrace(JSTracer* trc) {}

	ro::ConstString _type;
	bool _useCapture;
};	// EventListener

class JsFunctionEventListener : public EventListener
{
public:
	JsFunctionEventListener(JSContext* ctx);
	~JsFunctionEventListener();

	JSBool init(jsval stringOrFunc);

	void handleEvent(Event* evt, JSObject* self) override;

	jsval getJsVal();

protected:
	void* identifier() override { return JSVAL_TO_OBJECT(_jsClosure); }
	void jsTrace(JSTracer* trc) override;

	JSContext* _jsContext;
	jsval _jsClosure;		///< The js function closure to be invoked
	JSObject* _jsScript;	///< The compile script to be invoked
};	// JsFunctionEventListener

class ElementAttributeEventListener : public JsFunctionEventListener
{
public:
	ElementAttributeEventListener(JSContext* ctx, const char* eventAttributeName);

protected:
	void* identifier() override { return (void*)_eventAttributeName.hash(); }
	ro::ConstString _eventAttributeName;
};	// JsFunctionEventListener

class EventTarget
{
public:
	EventTarget();
	virtual ~EventTarget();

// Operations
	/// Registers a single event listener on this target.
	/// To register more than one event listeners for the target, addEventListener() for
	/// the same target but with different event types or capture parameters.
	void addEventListener(const char* type, EventListener* listener, bool useCapture);
	JSBool addEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture);
	JSBool addEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName, jsval stringOrFunc);

	void removeEventListener(const char* type, void* listenerIdentifier, bool useCapture);
	JSBool removeEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture);
	JSBool removeEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName);

	jsval getEventListenerAsAttribute(JSContext* cx, const char* eventAttributeName);

	void removeAllEventListener();

	/// Will perform capture and bubbling
	/// If self is NULL, then this pointer will pass as self to JS
	JSBool dispatchEvent(Event* evt, JSObject* self=NULL);

	void jsTrace(JSTracer* trc);

	virtual JSObject* getJSObject() = 0;

protected:
	bool _dispatchEventNoCaptureBubble(Event* evt, JSObject* self);

	/// Traverse up the DOM tree to the next EventTarget, returns NULL if the root is reached
	virtual EventTarget* eventTargetTraverseUp() { return NULL; }

	virtual void eventTargetAddReference() {}
	virtual void eventTargetReleaseReference() {}

// Attributes
public:
	bool hasListener() const { return !_eventListeners.isEmpty(); }

protected:
	typedef ro::LinkList<EventListener> EventListeners;
	EventListeners _eventListeners;
};	// EventTarget

class UIEvent : public Event
{
public:
};	// UIEvent

}	// namespace Dom

#endif	// __DOM_EVENT_H__
