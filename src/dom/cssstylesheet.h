#ifndef __DOM_CSSSTYLESHEET_H__
#define __DOM_CSSSTYLESHEET_H__

#include "cssstylerule.h"

namespace Dom {

/// https://developer.mozilla.org/en/DOM/CSSStyleSheet
/// http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSStyleSheet
class CSSStyleSheet : public JsBindable, public ro::ListNode<CSSStyleSheet>
{
public:
	explicit CSSStyleSheet(Rhinoca* rh);
	~CSSStyleSheet();

// Operations
	/// Insert a new rule after the specified index
	unsigned insertRule(const char* ruleString, unsigned index);

	void deleteRule(unsigned index);

	CSSStyleRule* getRuleAt(unsigned index);

	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	ro::LinkList<CSSStyleRule> rules;

	static JSClass jsClass;
};	// CSSStyleSheet

}	// namespace Dom

#endif	// __DOM_CSSSTYLESHEET_H__
