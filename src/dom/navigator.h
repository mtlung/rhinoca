#ifndef __DOM_NAVIGATOR_H__
#define __DOM_NAVIGATOR_H__

#include "../jsbindable.h"

namespace Dom {

class Window;

class Navigator : public JsBindable
{
public:
	Navigator();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

// Attributes
	ro::ConstString userAgent;

	static JSClass jsClass;
};	// Navigator

}	// namespace Dom

#endif	// __DOM_NAVIGATOR_H__
