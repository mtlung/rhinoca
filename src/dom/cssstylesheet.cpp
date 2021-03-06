#include "pch.h"
#include "cssstylesheet.h"
#include "cssparser.h"

using namespace ro::Parsing;

namespace Dom {

JSClass CSSStyleSheet::jsClass = {
	"CSSStyleSheet", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

CSSStyleSheet::CSSStyleSheet(Rhinoca* rh)
{
}

CSSStyleSheet::~CSSStyleSheet()
{
}

unsigned CSSStyleSheet::insertRule(const char* ruleString, unsigned index)
{
	CSSStyleRule* rule = new CSSStyleRule;

	rule->setCssText(ruleString);

	if(index >= rules.size()) {
		rules.pushBack(*rule);
		return rules.size() - 1;
	}
	else {
		rules.insertAfter(*rules.getAt(index), *rule);
		return index + 1;
	}
}

void CSSStyleSheet::deleteRule(unsigned index)
{
}

CSSStyleRule* CSSStyleSheet::getRuleAt(unsigned index)
{
	return rules.getAt(index);
}

void CSSStyleSheet::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
//	roVerify(JS_DefineFunctions(cx, *this, methods));
//	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
