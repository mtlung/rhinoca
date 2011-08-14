#include "pch.h"
#include "cssstylesheet.h"
#include "cssparser.h"

using namespace Parsing;

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

	if(index >= rules.elementCount()) {
		rules.pushBack(*rule);
		return rules.elementCount() - 1;
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
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
//	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
