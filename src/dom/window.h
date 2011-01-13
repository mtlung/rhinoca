#ifndef __DOME_WINDOW_H__
#define __DOME_WINDOW_H__

#include "../jsbindable.h"
#include "../map.h"
#include "../timer.h"

namespace Dom {

class Document;

class TimerCallback : public JsBindable, public MapBase<float>::Node<TimerCallback>
{
public:
	typedef MapBase<float>::Node<TimerCallback> super;

	~TimerCallback();

	void bind(JSContext* cx, JSObject* parent);

	void removeThis();

	float nextInvokeTime() const { return getKey(); }
	void setNextInvokeTime(float val) { setKey(val); }
	bool operator<(const TimerCallback& rhs) const { return nextInvokeTime() < rhs.nextInvokeTime(); }

	jsval closure;	///< The js function to be invoked
	float interval;	///< Will enqueue itself again if interval is larger than zero

	static JSClass jsClass;
};	// TimerCallback

/// Reference: http://www.w3schools.com/jsref/obj_window.asp
class Window : public JsBindable
{
public:
	explicit Window(Rhinoca* rh);
	~Window();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	void update();

// Attributes
	Rhinoca* rhinoca;
	Document* document;

	Timer timer;

	typedef Map<TimerCallback> TimerCallbacks;
	TimerCallbacks timerCallbacks;

	static JSClass jsClass;
};	// JsBindable

}	// namespace Dom

#endif	// __DOME_WINDOW_H__
