#ifndef __DOM_TOUCHEVENT_H__
#define __DOM_TOUCHEVENT_H__

#include "event.h"
#include "../vector.h"

namespace Dom {

// http://dvcs.w3.org/hg/webevents/raw-file/tip/touchevents.html

struct TouchData
{
// Attributes
	int index;	/// It's index to the touches array
	EventTarget* target;

	int identifier;
	int screenX, screenY;
	int clientX, clientY;
	int pageX, pageY;
};	// TouchData

class Touch : public JsBindable
{
public:
// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	TouchData data;

	static JSClass jsClass;
};	// Touch

class TouchList : public JsBindable
{
public:
// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	Vector<TouchData> touches;

	static JSClass jsClass;
};	// TouchList

class TouchEvent : public UIEvent
{
public:
	TouchEvent();
	~TouchEvent();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	Vector<TouchData> touches;
	Vector<TouchData> targetTouches;
	Vector<TouchData> changedTouches;

	static JSClass jsClass;
};	// TouchEvent

}	// namespace Dom

#endif	// __DOM_TOUCHEVENT_H__
