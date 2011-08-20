#include "pch.h"
#include "cssparser.h"

namespace Parsing {

// TODO: This number matching is not strict enough
bool NumberMatcher::match(Parser* p)
{
	const char* bk = p->begin;

	while(true) {
		char c = *p->begin;
		if( (c >= '0' && c <= '9') || c == '.') {
			p->begin++;
			continue;
		}

		break;
	}

	return bk < p->begin;
}

// TODO: match nonascii and escape
bool NameMatcher::match(Parser* p)
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

// TODO: match nonascii and escape
bool IdentMatcher::match(Parser* p)
{
	const char* bk = p->begin;

	char c = *p->begin;
	if( !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-') )
		return false;

	p->begin++;
	if(!name(p).once()) {
		p->begin = bk;
		return false;
	}

	return true;
}

bool HashMatcher::match(Parser* p)
{
	return character(p, '#').once() && name(p).once();
}

bool MediaMatcher::match(Parser* p)
{
	if(!string(p, "@media").once() ||
	   !skip(p).any() ||
	   !medium(p).once()
	)
		return false;
	
	while(
		character(p, ',').once() &&
		skip(p).any() &&
		medium(p).once()
	)
	{}

	return
		character(p, '{').once() &&
		skip(p).any() &&
		ruleSet(p).any() &&
		character(p, '}').once() &&
		skip(p).any();
}

bool MediumMatcher::match(Parser* p)
{
	return ident(p).once() && skip(p).any();
}

bool OperatorMatcher::match(Parser* p)
{
	return
		(character(p, '/').once() && skip(p).any()) ||
		(character(p, ',').once() && skip(p).any());
}

bool CombinatorMatcher::match(Parser* p)
{
	return
		(character(p, '+').once() && skip(p).any()) ||
		(character(p, '>').once() && skip(p).any());
}

bool UnaryOperatorMatcher::match(Parser* p)
{
	return character(p, '-').once() || character(p, '+').once();
}

bool PropertyMatcher::match(Parser* p)
{
	return ident(p).once() && skip(p).any();
}

bool ClassMatcher::match(Parser* p)
{
	return character(p, '.').once() && ident(p).once();
}

bool ElementNameMatcher::match(Parser* p)
{
	return ident(p).once() || character(p, '*').once();
}

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

bool CommentMatcher::match(Parser* p)
{
	const char* bk = p->begin;
	char lastChar = '\0';

	if(*(p->begin++) != '/') goto Fail;
	if(*(p->begin++) != '*') goto Fail;

	// Skip anything except "*/"
	lastChar = *p->begin;
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

bool AnyMatcher::match(Parser* p)
{
	return
		ident(p).once() ||
//		hex(p).once() ||
//		url(p).once() ||
//		quotedString(p).once() ||
//		doubleQuotedString(p).once() ||
		skip(p).any();
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
	if(!elementName(p).atMostOnce())
		return false;

	while(
		hash(p).once() ||
		klass(p).once()
	)
	{}

//	skip(p).any();

	return true;
}

bool SelectorMatcher::match(Parser* p)
{
	ParserResult selectorResult = { "selector", NULL, NULL };
	ParserResult combinatorResult = { "combinator", NULL, NULL };

	if(!simpleSelector(p).once(&selectorResult))
		return false;

	while(
		combinator(p).once(&combinatorResult) &&
		simpleSelector(p).once(&selectorResult)
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

bool PropertyDeclMatcher::match(Parser* p)
{
	ParserResult propName = { "propName", NULL, NULL };

	return
		skip(p).any() &&
		ident(p).once(&propName) &&
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
	return propertyDecls(p).once(&decls) && skip(p).any();
}

bool CssMatcher::match(Parser* p)
{
	ParserResult result = { "ruleSet", NULL, NULL };

	bool ret = false;
	while(skip(p).any() && (ruleSet(p).once(&result) || media(p).once()))
		ret = true;

	return ret;
}

}	// namespace Parsing
