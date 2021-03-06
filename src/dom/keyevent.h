#ifndef __DOM_KEYEVENT_H__
#define __DOM_KEYEVENT_H__

#include "event.h"

namespace Dom {

class KeyEvent : public UIEvent
{
public:
	KeyEvent();
	~KeyEvent();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	int keyCode;

	static JSClass jsClass;
};	// KeyEvent

}	// namespace Dom

#endif	// __DOM_KEYEVENT_H__
