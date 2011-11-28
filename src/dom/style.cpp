#include "pch.h"
#include "style.h"
#include "cssparser.h"
#include "cssstylesheet.h"
#include "document.h"
#include "textnode.h"

using namespace Parsing;

namespace Dom {

JSClass HTMLStyleElement::jsClass = {
	"HTMLStyleElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

HTMLStyleElement::HTMLStyleElement(Rhinoca* rh)
	: Element(rh)
	, source(NULL)
{
}

HTMLStyleElement::~HTMLStyleElement()
{
	free((void*)source);
}

void HTMLStyleElement::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
//	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
//	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

static void parserCallback(ParserResult* result, Parser* parser)
{
	CSSStyleSheet* style = reinterpret_cast<CSSStyleSheet*>(parser->userdata);

	if(strcmp(result->type, "ruleSet") != 0)
		return;

	char bk = *result->end;
	*const_cast<char*>(result->end) = '\0';
	style->insertRule(result->begin, style->rules.elementCount());
	*const_cast<char*>(result->end) = bk;
}

void HTMLStyleElement::onParserEndElement()
{
	TextNode* text = dynamic_cast<TextNode*>(firstChild);
	if(!text) return;

	CSSStyleSheet* css = new CSSStyleSheet(rhinoca);
	ownerDocument()->styleSheets.pushBack(*css);

	char* source = text->data.c_str();
	Parser parser(source, source + strlen(source), parserCallback, css);
	Parsing::css(&parser).once();
}

static const ro::ConstString _tagName = "STYLE";

Element* HTMLStyleElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, _tagName) == 0 ? new HTMLStyleElement(rh) : NULL;
}

const ro::ConstString& HTMLStyleElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
