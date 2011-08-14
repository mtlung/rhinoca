#ifndef __DOM_CSSSTYLEDECLARATION_H__
#define __DOM_CSSSTYLEDECLARATION_H__

#include "../jsbindable.h"

namespace Dom {

class CSSStyleRule;

/// http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSStyleDeclaration
class CSSStyleDeclaration : public JsBindable
{
public:
	explicit CSSStyleDeclaration();
	~CSSStyleDeclaration();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	float left, top;
	unsigned width, height;

	CSSStyleRule* parentRule;

	static JSClass jsClass;
};	// CSSStyleDeclaration

}	// namespace Dom

#endif	// __DOM_CSSSTYLEDECLARATION_H__
