#ifndef __DOM_MOUSEEVENT_H__
#define __DOM_MOUSEEVENT_H__

#include "../jsbindable.h"

namespace Dom {

class MouseEvent : public JsBindable
{
public:
	MouseEvent();
	~MouseEvent();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	int screenX, screenY;
	int clientX, clientY;
	int pageX, pageY;
	bool ctrlKey, shiftKey, altKey, metaKey;
	unsigned button;

	static JSClass jsClass;
};	// KeyEvent

}	// namespace Dom

#endif	// __DOM_MOUSEEVENT_H__
