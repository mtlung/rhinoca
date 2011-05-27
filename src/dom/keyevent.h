#ifndef __DOM_KEYEVENT_H__
#define __DOM_KEYEVENT_H__

#include "../jsbindable.h"

namespace Dom {

class KeyEvent : public JsBindable
{
public:
	KeyEvent();
	~KeyEvent();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	int keyCode;

	static JSClass jsClass;
};	// KeyEvent

}	// namespace Dom

#endif	// __DOM_KEYEVENT_H__
