#ifndef __DOM_CSSSTYLERULE_H__
#define __DOM_CSSSTYLERULE_H__

#include "cssstyledeclaration.h"
#include "../linklist.h"

namespace Dom {

class CSSStyleSheet;
class Element;

/// http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSStyleRule
class CSSStyleRule : public JsBindable, public LinkListBase::Node<CSSStyleRule>
{
public:
	explicit CSSStyleRule();
	~CSSStyleRule();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	/// Loop for the Element tree and assign the style for those element match with the selector.
	void selectorMatch(Element* tree);

// Attributes
	/// The parsable textual representation of the rule.
	/// This reflects the current state of the rule and not its initial value.
	const char* cssTest();
	void setCssText(const char* str);

	const char* selectorText();

	CSSStyleDeclaration style;

	CSSStyleSheet* parentStyleSheet();

	String _cssText;
	String _selectorText;

	static JSClass jsClass;
};	// CSSStyleRule

}	// namespace Dom

#endif	// __DOM_CSSSTYLERULE_H__
