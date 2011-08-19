#include "pch.h"
#include "cssstylerule.h"
#include "cssstylesheet.h"
#include "cssparser.h"
#include "element.h"
#include "elementstyle.h"
#include "../vector.h"

using namespace Parsing;

namespace Dom {

JSClass CSSStyleRule::jsClass = {
	"CSSStyleRule", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
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
	FixString type;
	const char* val;
	int len;
};

struct SelectionBuffer
{
	Vector<SingleSelectorState> state;
};

static void selectorParserCallback(ParserResult* result, Parser* parser)
{
	SelectionBuffer* state = reinterpret_cast<SelectionBuffer*>(parser->userdata);

	SingleSelectorState ss = { result->type, result->begin, result->end - result->begin };

	state->state.push_back(ss);
}

static bool hashCompare(const FixString& str1, const char* str2, unsigned len)
{
	StringLowerCaseHash hash(str2, len);
	return str1.lowerCaseHashValue() == hash;
}

static bool match(const SingleSelectorState& state, Element* ele)
{
	switch(state.val[0])
	{
	case '#':	// Match id
		ASSERT(state.len > 1);
		if(hashCompare(ele->id, state.val + 1, state.len - 1))
			return true;
		break;
	case '.':	// Match class name
		ASSERT(state.len > 1);
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

const char* CSSStyleRule::cssTest()
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
