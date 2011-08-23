#include "pch.h"
#include "cssparser.h"

namespace Parsing {

static bool _CdoCdc(Parser* p)
{
	bool ret = false;
	while(skip(p).once() || string(p, "<!--").once() || string(p, "-->").once())
		ret = true;
	return ret;
}

bool CssMatcher::match(Parser* p)
{
	ParserResult result = { "ruleSet", NULL, NULL };

	_CdoCdc(p);

	while(
		*p->begin != '\0' &&
		(
			ruleSet(p).once(&result) ||
			media(p).once() ||
			_CdoCdc(p)
		)
	)
	{}

	return true;
}

bool MediaMatcher::match(Parser* p)
{
	if(!string(p, "@media").once() ||
	   !skip(p).any() ||
	   !mediaQueryList(p).once()
	)
		return false;

	return
		character(p, '{').once() &&
		skip(p).any() &&
		ruleSet(p).any() &&
		character(p, '}').once() &&
		skip(p).any();
}

static bool andMediaExpression(Parser* p)
{
	return
		string(p, "and").once() &&
		skip(p).any() &&
		mediaExpression(p).once();
}

bool MediaQueryListMatcher::match(Parser* p)
{
	skip(p).any();

	if(mediaQuery(p).once()) while(
		character(p, ',').once() &&
		skip(p).any() &&
		mediaQuery(p).once()
	)
	{}

	return true;
}

bool MediaQueryMatcher::match(Parser* p)
{
	if(mediaExpression(p).once()) {
		while(andMediaExpression(p)) {}
		return true;
	}

	string(p, "only").once() || string(p, "not").once();
	skip(p).any();

	if(!mediaType(p).once())
		return false;

	skip(p).any();
	while(andMediaExpression(p)) {}

	return true;
}

bool MediaTypeMatcher::match(Parser* p)
{
	return ident(p).once();
}

bool MediaExpressionMatcher::match(Parser* p)
{
	if(!(
		character(p, '(').once() &&
		skip(p).any() && 
		mediaFeature(p).once() &&
		skip(p).any())
	)
		return false;

	character(p, ':').once() && skip(p).any() && expr(p).once();

	if(!character(p, ')').once())
		return false;

	skip(p).any();
	return true;
}

bool MediaFeatureMatcher::match(Parser* p)
{
	return ident(p).once();
}

bool OperatorMatcher::match(Parser* p)
{
	(character(p, '/').once() && skip(p).any()) ||
	(character(p, ',').once() && skip(p).any());

	return true;	// can be empty
}

bool CombinatorMatcher::match(Parser* p)
{
	(character(p, '+').once() && skip(p).any()) ||
	(character(p, '>').once() && skip(p).any());

	return true;	// can be empty
}

bool UnaryOperatorMatcher::match(Parser* p)
{
	return character(p, '-').once() || character(p, '+').once();
}

bool PropertyMatcher::match(Parser* p)
{
	ParserResult propNameResult = { "propName", NULL, NULL };
	return ident(p).once(&propNameResult) && skip(p).any();
}

bool PropertyValueMatcher::match(Parser* p)
{
	return expr(p).once() && prio(p).atMostOnce();

//	return anyCharExcept(p, ";}").atLeastOnce();
}

bool RuleSetMatcher::match(Parser* p)
{
	ParserResult sGroup = { "selectors", NULL, NULL };

	if(!selectors(p).once(&sGroup))
		return false;

	ParserResult decls = { "decls", NULL, NULL };
	return declarations(p, true).once(&decls);
}

bool SelectorMatcher::match(Parser* p)
{
	ParserResult selectorResult = { "selector", NULL, NULL };
	ParserResult combinatorResult = { "combinator", NULL, NULL };

	if(!simpleSelector(p).once(&selectorResult) || !skip(p).any())
		return false;

	while(
		combinator(p).once(&combinatorResult) &&
		simpleSelector(p).once(&selectorResult) &&
		skip(p).any()
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

bool SimpleSelectorMatcher::match(Parser* p)
{
	bool ret = false;

	ret = elementName(p).once();

	while(
		hash(p).once() ||
		klass(p).once() ||
		attrib(p).once() ||
		pseudo(p).once()
	)
	{
		ret |= true;
	}

	return ret;
}

bool ClassMatcher::match(Parser* p)
{
	return character(p, '.').once() && ident(p).once();
}

bool ElementNameMatcher::match(Parser* p)
{
	return ident(p).once() || character(p, '*').once();
}

bool AttribMatcher::match(Parser* p)
{
	if( !character(p, '[').once() ||
		!skip(p).any() ||
		ident(p).once() ||
		!skip(p).any()
	)
		return false;

	(character(p, '=').once() || string(p, "~=").once() || string(p, "|=").once()) &&
	skip(p).any() &&
	(ident(p).once() || doubleQuotedString(p).once()) &&
	skip(p).any();

	return character(p, ']').once();
}

bool PseudoMatcher::match(Parser* p)
{
	if(!character(p, ':').once() && skip(p).any())
		return false;

	if(	ident(p).once() &&
		skip(p).any() &&
		character(p, '(').once() &&
		skip(p).any() &&
		ident(p).once() && 
		character(p, ')').once() &&
		skip(p).any()
	)
		return true;

	return ident(p).once() && skip(p).any();
}

bool DeclarationMatcher::match(Parser* p)
{
	ParserResult propValueResult = { "propVal", NULL, NULL };

	property(p).once() &&
	character(p, ':').once() &&
	skip(p).any() &&
	propertyValue(p).once(&propValueResult);

	return true;	// can be empty
}

bool DeclarationsMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	if(includeCurryBracket)
	if(!character(p, '{').once() || !skip(p).any()) {
		p->reportError("missing '{'");
		return false;
	}

	result.type = "decls";
	const char* retBegin = p->begin;

	if(!declaration(p).once()) {
		p->reportError("no property declared");
		return false;
	}

	while(
		character(p, ';').once() &&
		skip(p).any() &&
		declaration(p).once()
	)
	{}

	const char* retEnd = p->begin;

	if(includeCurryBracket)
	if(!character(p, '}').once()) {
		p->reportError("missing '}'");
		return false;
	}

	result.begin = retBegin;
	result.end =retEnd;

	skip(p).any();

	return true;
}

bool PrioMatcher::match(Parser* p)
{
	return
		character(p, '!').once() && skip(p).any() &&
		!string(p, "important").once() && skip(p).any();
}

bool ExprMatcher::match(Parser* p)
{
	if(!term(p).once())
		return false;

	while(
		operatar(p).once() &&
		term(p).once()
	)
	{}

	return true;
}

bool TermMatcher::match(Parser* p)
{
	return
	(
		unaryOperator(p).atMostOnce() &&
		(
			( unit(p).once() && skip(p).any() ) ||
			( function(p).once() && skip(p).any() )
		)
	) ||
	(
		( url(p).once() && skip(p).any() ) ||
		( hex(p).once() && skip(p).any() ) ||
		( ident(p).once() && skip(p).any() )
	);
}

bool FunctionMatcher::match(Parser* p)
{
	return
		ident(p).once() &&
		skip(p).any() &&
		character(p, '(').once() &&
		skip(p).any() &&
		expr(p).once() && 
		character(p, ')').once() &&
		skip(p).any();
}

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
	if(!name(p).atMostOnce()) {
		p->begin = bk;
		return false;
	}

	return true;
}

bool HashMatcher::match(Parser* p)
{
	return character(p, '#').once() && name(p).once();
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

bool UnitMatcher::match(Parser* p)
{
	if(!number(p).once() && skip(p).any())
		return false;

	string(p, "px").once() ||
	string(p, "cm").once() ||
	string(p, "pt").once() ||
	string(p, "deg").once() ||
	string(p, "rad").once() ||
	string(p, "grad").once() ||
	string(p, "ms").once() ||
	string(p, "s").once() ||
	string(p, "hz").once() ||
	string(p, "khz").once() ||
	string(p, "%").once();

	return true;
}

bool HexMatcher::match(Parser* p)
{
	ParserResult result = { "hex", NULL, NULL };

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

}	// namespace Parsing
