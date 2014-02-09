#ifndef __DOM_MOUSEEVENT_H__
#define __DOM_MOUSEEVENT_H__

#include "event.h"

namespace Dom {

class MouseEvent : public UIEvent
{
public:
	MouseEvent();
	~MouseEvent();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	int screenX, screenY;
	int clientX, clientY;
	int pageX, pageY;
	bool ctrlKey, shiftKey, altKey, metaKey;
	unsigned button;	/// 1: left button, 2: middle button, 3: right button

	static JSClass jsClass;
};	// KeyEvent

}	// namespace Dom

#endif	// __DOM_MOUSEEVENT_H__
