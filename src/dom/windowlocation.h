#ifndef __DOM_WINDOWLOCATION_H__
#define __DOM_WINDOWLOCATION_H__

#include "../jsbindable.h"

namespace Dom {

class Window;

/// Reference: http://www.w3schools.com/jsref/obj_location.asp
class WindowLocation : public JsBindable
{
public:
	explicit WindowLocation(Window* window);

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	Window* window;

	static JSClass jsClass;
};	// WindowLocation

}	// namespace Dom

#endif	// __DOM_WINDOWLOCATION_H__
