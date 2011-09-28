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
	const char* cssText();
	void setCssText(const char* str);

	const char* selectorText();

	CSSStyleDeclaration style;

	CSSStyleSheet* parentStyleSheet();

	String _cssText;
	const char* _selectorTextBegin;	// Pointer to the memory of _cssText
	const char* _selectorTextEnd;	//
	const char* _declsTextBegin;	// Pointer to the memory of _cssText
	const char* _declsTextEnd;		//

	static JSClass jsClass;
};	// CSSStyleRule

}	// namespace Dom

#endif	// __DOM_CSSSTYLERULE_H__
