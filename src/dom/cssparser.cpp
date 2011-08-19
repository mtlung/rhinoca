#include "pch.h"
#include "cssparser.h"

namespace Parsing {

bool UrlMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	result.type = "url";
	bool hasParenthesis = false;

	return
		whiteSpace(p).any() &&
		string(p, "url").once() &&
		(	whiteSpace(p).atLeastOnce() ||
			(hasParenthesis = character(p, '(').once())
		) &&
		whiteSpace(p).any() &&
		(	quotedString(p).once(&result) ||
			doubleQuotedString(p).once(&result) ||
			anyCharExcept(p, " \t\n\r,()'\"").any(&result)
		) &&
		whiteSpace(p).any() &&
		(	hasParenthesis ?
			character(p, ')').once() : true
		);
}

bool HexMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	return
		whiteSpace(p).any() &&
		character(p, '#').once() &&
		digit(p).any(&result);
}

bool IdentifierMatcher::match(Parser* p)
{
	const char* bk = p->begin;

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

bool CommentMatcher::match(Parser* p)
{
	const char* bk = p->begin;

	if(*(p->begin++) != '/') goto Fail;
	if(*(p->begin++) != '*') goto Fail;

	// Skip anything except "*/"
	char lastChar = *p->begin;
	for(const char* c=p->begin+1; c<p->end; ++c)
	{
		if(lastChar == '*' && *c == '/') {
			p->begin = c + 1;
			return true;
		}
		lastChar = *c;
	}

Fail:
	p->begin = bk;
	return false;
}

bool SkippableMatcher::match(Parser* p)
{
	return whiteSpace(p).once() || comment(p).once();
}

bool PropertyValueMatcher::match(Parser* p)
{
	ParserResult result = { "propVal", NULL, NULL };

	return
		skip(p).any() &&
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

	if(!(skip(p).any() && simpleSelector(p).once(&selector)))
		return false;

	while(
		skip(p).any() &&
		(	character(p, '+').once(&sSibling) ||
			character(p, '>').once(&sChild) ||
			true	// '|| true' make this block optional, but will this be optimized away be compiler?
		) &&
		skip(p).any() &&
		simpleSelector(p).once(&selector)
	)
	{}

	return true;
}

bool SelectorsMatcher::match(Parser* p)
{
	if(!selector(p).once())
		return false;

	ParserResult sGroup = { "sGroup", NULL, NULL };

	while(skip(p).any() && character(p, ',').once(&sGroup) && skip(p).any() && selector(p).once())
	{}

	return true;
}

bool MediumMatcher::match(Parser* p)
{
	return
		string(p, "all").once() ||
		string(p, "aural").once() ||
		string(p, "braille").once() ||
		string(p, "embossed").once() ||
		string(p, "handheld").once() ||
		string(p, "print").once() ||
		string(p, "projection").once() ||
		string(p, "screen").once() ||
		string(p, "tty").once() ||
		string(p, "tv").once();
}

bool PropertyDeclMatcher::match(Parser* p)
{
	ParserResult propName = { "propName", NULL, NULL };

	return
		skip(p).any() &&
		identifier(p).once(&propName) &&
		skip(p).any() &&
		character(p, ':').once() &&
		skip(p).any() &&
		propertyValue(p).once() &&
		skip(p).any() &&
		character(p, ';').atMostOnce();
}

bool PropertyDeclsMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	if(!skip(p).any() || !character(p, '{').once()) {
		p->reportError("missing '{'");
		return false;
	}

	result.type = "decls";

	skip(p).any();
	if(!propertyDecl(p).atLeastOnce(&result)) {
		p->reportError("no property declared");
		return false;
	}

	if(!skip(p).any() || !character(p, '}').once()) {
		p->reportError("missing '}'");
		return false;
	}

	return true;
}

bool RuleSetMatcher::match(Parser* p)
{
	ParserResult sGroup = { "selectors", NULL, NULL };

	if(!selectors(p).once(&sGroup))
		return false;

	ParserResult decls = { "decls", NULL, NULL };
	return propertyDecls(p).once(&decls);
}

bool CssMatcher::match(Parser* p)
{
	ParserResult result = { "ruleSet", NULL, NULL };

	bool ret = false;
	while(skip(p).any() && ruleSet(p).once(&result))
		ret = true;

	return ret;
}

}	// namespace Parsing
