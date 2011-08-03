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

bool CommentMatcher::match(Parser* p)
{
	char* bk = p->begin;

	if(*(p->begin++) != '/') goto Fail;
	if(*(p->begin++) != '*') goto Fail;

	// Skip anything except "*/"
	char lastChar = *p->begin;
	for(char* c=p->begin+1; c<p->end; ++c)
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

bool RuleSetMatcher::match(Parser* p)
{
	if(!selector(p).once())
		return false;

	ParserResult sGroup = { "sGroup", NULL, NULL };

	while(skip(p).any() && character(p, ',').once(&sGroup) && skip(p).any() && selector(p).once())
	{}

	if(!skip(p).any() || !character(p, '{').once()) {
		p->reportError("missing '{'");
		return false;
	}

	if(!propertyDecl(p).atLeastOnce()) {
		p->reportError("no property declared");
		return false;
	}

	if(!skip(p).any() || !character(p, '}').once()) {
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
