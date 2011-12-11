#ifndef __DOM_WINDOW_H__
#define __DOM_WINDOW_H__

#include "touchevent.h"
#include "../jsbindable.h"
#include "../linklist.h"
#include "../../roar/base/roArray.h"
#include "../../roar/base/roMap.h"
#include "../../roar/base/roStopWatch.h"

namespace Dom {

class Event;
class HTMLDocument;
class HTMLCanvasElement;

class TimerCallback : public JsBindable, public ro::MapNode<float, TimerCallback>
{
public:
	typedef ro::MapNode<float, TimerCallback> super;

	TimerCallback();
	~TimerCallback();

	override void bind(JSContext* cx, JSObject* parent);

	void removeThis();

	float nextInvokeTime() const { return key(); }
	void setNextInvokeTime(float val) { setKey(val); }
	bool operator<(const TimerCallback& rhs) const { return nextInvokeTime() < rhs.nextInvokeTime(); }

	jsval closure;		///< The js function closure to be invoked
	JSObject* jsScript;	///< The compile script to be invoked
	float interval;		///< Will enqueue itself again if interval is larger than zero

	static JSClass jsClass;
};	// TimerCallback

// http://webstuff.nfshost.com/anim-timing/Overview.html
class FrameRequestCallback : public JsBindable, public LinkListBase::Node<FrameRequestCallback>
{
public:
	typedef LinkListBase::Node<FrameRequestCallback> super;

	FrameRequestCallback();
	~FrameRequestCallback();

	override void bind(JSContext* cx, JSObject* parent);

	void removeThis();

	jsval closure;				///< The js function closure to be invoked
	JSObject* thatObject;

	static JSClass jsClass;
};	// FrameRequestCallback

/// Reference: http://www.w3schools.com/jsref/obj_window.asp
class Window : public JsBindable, public EventTarget
{
public:
	explicit Window(Rhinoca* rh);
	~Window();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	void dispatchEvent(Event* e);

	void update();

	void render();

	TouchData& allocateTouchData(int identifier);

	/// Reutrn -1 if no such identifier found
	int findTouchIndexByIdentifier(int identifier) const;

	static void registerClass(JSContext* cx, JSObject* parent);

// Attributes
	Rhinoca* rhinoca;
	HTMLDocument* document;

	unsigned width() const;
	unsigned height() const;

	ro::StopWatch stopWatch;

	/// We use this canvas for rendering stuffs into the window
	HTMLCanvasElement* virtualCanvas;

	Event* currentEvent;	/// Carry the Event object when a event is risen

	ro::Array<TouchData> touches;	/// Book keeping for all touch points

	typedef ro::Map<TimerCallback> TimerCallbacks;
	TimerCallbacks timerCallbacks;

	typedef LinkList<FrameRequestCallback> FrameRequestCallbacks;
	FrameRequestCallbacks frameRequestCallbacks;

	static JSClass jsClass;

protected:
	override JSObject* getJSObject() { return jsObject; }
};	// Window

}	// namespace Dom

#endif	// __DOM_WINDOW_H__
