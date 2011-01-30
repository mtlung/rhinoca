#ifndef __DOME_WINDOW_H__
#define __DOME_WINDOW_H__

#include "../jsbindable.h"
#include "../map.h"
#include "../timer.h"

namespace Dom {

class HTMLDocument;
class HTMLCanvasElement;

class TimerCallback : public JsBindable, public MapBase<float>::Node<TimerCallback>
{
public:
	typedef MapBase<float>::Node<TimerCallback> super;

	TimerCallback();
	~TimerCallback();

	void bind(JSContext* cx, JSObject* parent);

	void removeThis();

	float nextInvokeTime() const { return getKey(); }
	void setNextInvokeTime(float val) { setKey(val); }
	bool operator<(const TimerCallback& rhs) const { return nextInvokeTime() < rhs.nextInvokeTime(); }

	jsval closure;				///< The js function closure to be invoked
	JSScript* jsScript;			///< The compile script to be invoked
	JSObject* jsScriptObject;	///< To manage the life-time of jsScript
	float interval;				///< Will enqueue itself again if interval is larger than zero

	static JSClass jsClass;
};	// TimerCallback

/// Reference: http://www.w3schools.com/jsref/obj_window.asp
class DOMWindow : public JsBindable
{
public:
	explicit DOMWindow(Rhinoca* rh);
	~DOMWindow();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	void update();

	void render();

// Attributes
	Rhinoca* rhinoca;
	HTMLDocument* document;

	unsigned width() const;
	unsigned height() const;

	Timer timer;

	/// We use this canvas for rendering stuffs into the window
	HTMLCanvasElement* virtualCanvas;

	typedef Map<TimerCallback> TimerCallbacks;
	TimerCallbacks timerCallbacks;

	static JSClass jsClass;
};	// JsBindable

}	// namespace Dom

#endif	// __DOME_WINDOW_H__
