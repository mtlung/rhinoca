#ifndef __DOM_TOUCHEVENT_H__
#define __DOM_TOUCHEVENT_H__

#include "event.h"

namespace Dom {

class TouchEvent : public UIEvent
{
public:
	TouchEvent();
	~TouchEvent();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	int screenX, screenY;
	int clientX, clientY;
	int pageX, pageY;

	static JSClass jsClass;
};	// TouchEvent

}	// namespace Dom

#endif	// __DOM_TOUCHEVENT_H__
