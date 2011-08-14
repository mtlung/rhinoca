#include "pch.h"
#include "cssstylerule.h"
#include "cssstylesheet.h"
#include "cssparser.h"

using namespace Parsing;

namespace Dom {

JSClass CSSStyleRule::jsClass = {
	"CSSStyleRule", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

CSSStyleRule::CSSStyleRule()
{
}

CSSStyleRule::~CSSStyleRule()
{
}

static void parserCallback(ParserResult* result, Parser* parser)
{
	CSSStyleRule* rule = reinterpret_cast<CSSStyleRule*>(parser->userdata);

	if(strcmp(result->type, "selectors") == 0) {
		rule->_selectorText.assign(result->begin, result->end - result->begin);
	}
}

const char* CSSStyleRule::cssTest()
{
	return _cssText.c_str();
}

void CSSStyleRule::setCssText(const char* str)
{
	_cssText = str;

	Parser parser(_cssText.c_str(), _cssText.c_str() + _cssText.size(), parserCallback, this);
	Parsing::ruleSet(&parser).once();
}

void CSSStyleRule::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
//	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

CSSStyleSheet* CSSStyleRule::parentStyleSheet()
{
	// Get CSSStyleSheet base on CSSStyleSheet::rules
	LinkListBase* list = getList();
	return (CSSStyleSheet*)
	((intptr_t(list) + 1) - intptr_t(&((CSSStyleSheet*)1)->rules));
}

}	// namespace Dom
