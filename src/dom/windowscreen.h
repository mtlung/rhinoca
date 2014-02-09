#ifndef __DOM_WINDOWSCREEN_H__
#define __DOM_WINDOWSCREEN_H__

#include "../jsbindable.h"

namespace Dom {

class Window;

/// Reference: http://www.w3schools.com/jsref/obj_screen.asp
class WindowScreen : public JsBindable
{
public:
	explicit WindowScreen(Window* window);

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	Window* window;

	unsigned width() const;
	unsigned height() const;
	unsigned availWidth() const;
	unsigned availHeight() const;

	static JSClass jsClass;
};	// WindowScreen

}	// namespace Dom

#endif	// __DOM_WINDOWSCREEN_H__
