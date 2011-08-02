#include "pch.h"
#include "cssparser.h"

namespace Parsing {

bool UrlMatcher::match(Parser* p)
{
	p->result.type = "url";
	bool hasParenthesis = false;

	return
		whiteSpace(p).any() &&
		string(p, "url").once() &&
		(	whiteSpace(p).atLeastOnce() ||
			(hasParenthesis = character(p, '(').once())
		) &&
		whiteSpace(p).any() &&
		(	quotedString(p).once(&p->result) ||
			doubleQuotedString(p).once(&p->result) ||
			anyCharExcept(p, " \t\n\r,()'\"").any(&p->result)
		) &&
		whiteSpace(p).any() &&
		(	hasParenthesis ?
			character(p, ')').once() : true
		);
}

bool HexMatcher::match(Parser* p)
{
	return
		whiteSpace(p).any() &&
		character(p, '#').once() &&
		digit(p).any(&p->result);
}

bool IdentifierMatcher::match(Parser* p)
{
	char* bk = p->begin;

	while(true) {
		char c = *p->begin;
		if( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
			p->begin++;
			continue;
		}

		break;
	}

	return bk < p->begin;
}

bool PropertyValueMatcher::match(Parser* p)
{
	ParserResult result = { "propVal", NULL, NULL };

	return
		whiteSpace(p).any() &&
		anyCharExcept(p, ";}").atLeastOnce(&result);
}

bool SimpleSelectorMatcher::match(Parser* p)
{
	bool ret =
		(character(p, '*').once() || identifier(p).once());

	while(
		(character(p, '#').once() && identifier(p).once()) ||
		(character(p, '.').once() && identifier(p).once())
	)
	{
		ret = true;
	}

	return ret;
}

bool SelectorMatcher::match(Parser* p)
{
	ParserResult selector = { "selector", NULL, NULL };
	ParserResult sSibling = { "sSibling", NULL, NULL };
	ParserResult sChild = { "sChild", NULL, NULL };

	if(!(whiteSpace(p).any() && simpleSelector(p).once(&selector)))
		return false;

	while(
		whiteSpace(p).any() &&
		(	character(p, '+').once(&sSibling) ||
			character(p, '>').once(&sChild) ||
			true	// '|| true' make this block optional, but will this be optimized away be compiler?
		) &&
		whiteSpace(p).any() &&
		simpleSelector(p).once(&selector)
	)
	{}

	return true;
}

bool PropertyDeclMatcher::match(Parser* p)
{
	ParserResult propName = { "propName", NULL, NULL };

	return
		whiteSpace(p).any() &&
		identifier(p).once(&propName) &&
		whiteSpace(p).any() &&
		character(p, ':').once() &&
		whiteSpace(p).any() &&
		propertyValue(p).once() &&
		whiteSpace(p).any() &&
		character(p, ';').atMostOnce();
}

bool RuleSetMatcher::match(Parser* p)
{
	if(!selector(p).once())
		return false;

	ParserResult sGroup = { "sGroup", NULL, NULL };

	while(whiteSpace(p).any() && character(p, ',').once(&sGroup) && whiteSpace(p).any() && selector(p).once())
	{}

	if(!whiteSpace(p).any() || !character(p, '{').once()) {
		p->reportError("missing '{'");
		return false;
	}

	if(!propertyDecl(p).atLeastOnce()) {
		p->reportError("no property declared");
		return false;
	}

	if(!whiteSpace(p).any() || !character(p, '}').once()) {
		p->reportError("missing '}'");
		return false;
	}

	return true;
}

bool CssMatcher::match(Parser* p)
{
	return ruleSet(p).atLeastOnce();
}

}	// namespace Parsing
