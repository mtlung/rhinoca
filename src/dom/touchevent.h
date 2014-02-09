#ifndef __DOM_TOUCHEVENT_H__
#define __DOM_TOUCHEVENT_H__

#include "event.h"
#include "../../roar/base/roArray.h"

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
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	TouchData data;

	static JSClass jsClass;
};	// Touch

class TouchList : public JsBindable
{
public:
// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	ro::Array<TouchData> touches;

	static JSClass jsClass;
};	// TouchList

class TouchEvent : public UIEvent
{
public:
	TouchEvent();
	~TouchEvent();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	ro::Array<TouchData> touches;
	ro::Array<TouchData> targetTouches;
	ro::Array<TouchData> changedTouches;

	static JSClass jsClass;
};	// TouchEvent

}	// namespace Dom

#endif	// __DOM_TOUCHEVENT_H__
