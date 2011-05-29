#ifndef __DOM_EVENT_H__
#define __DOM_EVENT_H__

#include "../jsbindable.h"
#include "../linklist.h"

// Documentation for DOM event model:
// http://www.w3.org/TR/DOM-Level-2-Events/events.html

namespace Dom {

class EventTarget;

class Event : public JsBindable
{
public:
	Event();

// Operations
	JSObject* createPrototype();

// Attributes
	enum Phase {
		NONE = 0,
		CAPTURING_PHASE = 1,
		AT_TARGET = 2,
		BUBBLING_PHASE = 3
	};

	FixString type;
	Phase eventPhase;
	EventTarget* target;
	EventTarget* currentTarget;
	bool bubbles;
	bool cancelable;

	static JSClass jsClass;
};	// Event

class EventListener : public LinkListBase::Node<EventListener>
{
public:
	virtual void handleEvent(Event* evt) = 0;

protected:
	friend class EventTarget;

	/// The signature of the EventListener
	virtual void* identifier() { return this; }
	virtual void jsTrace(JSTracer* trc) {}

	FixString _type;
	bool _useCapture;
};	// EventListener

class JsFunctionEventListener : public EventListener
{
public:
	JsFunctionEventListener(JSContext* ctx, jsval closure);

	~JsFunctionEventListener();

	override void handleEvent(Event* evt);

protected:
	override void* identifier() { return (void*)_jsClosure; }
	override void jsTrace(JSTracer* trc);

	JSContext* _jsContext;
	jsval _jsClosure;	///< The js function closure to be invoked
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

	void removeEventListener(const char* type, void* listenerIdentifier, bool useCapture);
	JSBool removeEventListener(JSContext* cx, jsval type, jsval func, jsval useCapture);

	void removeAllEventListener();

	bool dispatchEvent(Event* evt);
	JSBool dispatchEvent(JSContext* cx, jsval evt);

	void jsTrace(JSTracer* trc);

protected:
	/// Traverse up the DOM tree to the next EventTarget, returns NULL if the root is reached
	virtual EventTarget* eventTargetTraverseUp() = 0;

	virtual void eventTargetAddReference() = 0;
	virtual void eventTargetReleaseReference() = 0;

// Attributes
protected:
	typedef LinkList<EventListener> EventListeners;
	EventListeners _eventListeners;
};	// EventTarget

class UIEvent : public Event
{
public:
};	// UIEvent

}	// namespace Dom

#endif	// __DOM_EVENT_H__
