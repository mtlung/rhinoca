#include "pch.h"
#include "cssstylerule.h"
#include "cssstylesheet.h"
#include "cssparser.h"
#include "element.h"
#include "elementstyle.h"
#include "../../roar/base/roArray.h"
#include "../../roar/base/roStringHash.h"

using namespace Parsing;

namespace Dom {

JSClass CSSStyleRule::jsClass = {
	"CSSStyleRule", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool cssText(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CSSStyleRule* self = getJsBindable<CSSStyleRule>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->cssText()));
	return JS_TRUE;
}

static JSBool selectorText(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CSSStyleRule* self = getJsBindable<CSSStyleRule>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyN(cx, self->_selectorTextBegin, self->_selectorTextEnd - self->_selectorTextBegin));
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"cssText", 0, JSPROP_READONLY | JsBindable::jsPropFlags, cssText, JS_StrictPropertyStub},
	{"selectorText", 0, JSPROP_READONLY | JsBindable::jsPropFlags, selectorText, JS_StrictPropertyStub},
	{0}
};

CSSStyleRule::CSSStyleRule()
	: _selectorTextBegin(NULL)
	, _selectorTextEnd(NULL)
	, _declsTextBegin(NULL)
	, _declsTextEnd(NULL)
{
}

CSSStyleRule::~CSSStyleRule()
{
}

struct SingleSelectorState
{
	ro::ConstString type;
	const char* val;
	int len;
};

struct SelectionBuffer
{
	ro::Array<SingleSelectorState> state;
};

static void selectorParserCallback(ParserResult* result, Parser* parser)
{
	SelectionBuffer* state = reinterpret_cast<SelectionBuffer*>(parser->userdata);

	SingleSelectorState ss = { result->type, result->begin, result->end - result->begin };

	state->state.pushBack(ss);
}

static bool hashCompare(const ro::ConstString& str1, const char* str2, unsigned len)
{
	const ro::StringHash hash = ro::stringLowerCaseHash(str2, len);
	return str1.lowerCaseHash() == hash;
}

static bool match(const SingleSelectorState& state, Element* ele)
{
	switch(state.val[0])
	{
	case '#':	// Match id
		RHASSERT(state.len > 1);
		if(hashCompare(ele->id, state.val + 1, state.len - 1))
			return true;
		break;
	case '.':	// Match class name
		RHASSERT(state.len > 1);
		if(hashCompare(ele->className, state.val + 1, state.len - 1))
			return true;
		break;
	default:	// Match tag name
		if(hashCompare(ele->tagName(), state.val, state.len))
			return true;
		break;
	}

	return false;
}

void CSSStyleRule::selectorMatch(Element* tree)
{
	if(!tree)
		return;

	// Construct the selector representation buffer
	SelectionBuffer state;
	Parser parser(_selectorTextBegin, _selectorTextEnd, selectorParserCallback, &state);
	Parsing::selector(&parser).once();

	// Perform selection on each node
	for(NodeIterator i(tree); !i.ended(); i.next())
	{
		Element* ele = dynamic_cast<Element*>(i.current());
		if(!ele)
			continue;

		// We traverse the selectors in reverse order
		for(unsigned j=state.state.size(); j--;)
		{
			if(match(state.state[j], ele)) {
				// Assign style
				ele->style()->setStyleString(_declsTextBegin, _declsTextEnd);
			}
		}
	}
}

static void cssParserCallback(ParserResult* result, Parser* parser)
{
	CSSStyleRule* rule = reinterpret_cast<CSSStyleRule*>(parser->userdata);

	if(strcmp(result->type, "selectors") == 0) {
		rule->_selectorTextBegin = result->begin;
		rule->_selectorTextEnd = result->end;
	}
	else if(strcmp(result->type, "decls") == 0) {
		rule->_declsTextBegin = result->begin;
		rule->_declsTextEnd = result->end;
	}
}

const char* CSSStyleRule::cssText()
{
	return _cssText.c_str();
}

void CSSStyleRule::setCssText(const char* str)
{
	_cssText = str;

	Parser parser(_cssText.c_str(), _cssText.c_str() + _cssText.size(), cssParserCallback, this);
	Parsing::ruleSet(&parser).once();
}

void CSSStyleRule::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
//	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

CSSStyleSheet* CSSStyleRule::parentStyleSheet()
{
	// Get CSSStyleSheet base on CSSStyleSheet::rules
	ro::_LinkList* list = getList();
	return (CSSStyleSheet*)
	((intptr_t(list) + 1) - intptr_t(&((CSSStyleSheet*)1)->rules));
}

}	// namespace Dom
