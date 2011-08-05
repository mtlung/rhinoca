#ifndef __DOM_CSSPARSER_H__
#define __DOM_CSSPARSER_H__

#include "../parser.h"

namespace Parsing {

/// CSS selector syntax: http://www.w3.org/TR/CSS2/selector.html#selector-syntax
/// See http://www.codeproject.com/KB/recipes/CSSParser.aspx
/// http://stackoverflow.com/questions/4656975/use-css-selectors-to-collect-html-elements-from-a-streaming-parser-e-g-sax-stre

/// EBNF language used inside the comment:
/// Alternatives: |, group by ()
/// Repetitions: {}0-* {}1-*
/// Options: []

/// Match url('http://abc.com') | url("http://abc.com") | url(http://abc.com)
struct UrlMatcher
{
	bool match(Parser* parser);
};

inline Matcher<UrlMatcher> url(Parser* parser) {
	Matcher<UrlMatcher> ret = { {}, parser };
	return ret;
}

/// Match #123ADF
struct HexMatcher
{
	bool match(Parser* parser);
};

inline Matcher<HexMatcher> hex(Parser* parser) {
	Matcher<HexMatcher> ret = { {}, parser };
	return ret;
}

/// Match { A-Z|a-z|0-9|'-' }1-*
struct IdentifierMatcher
{
	bool match(Parser* parser);
};

inline Matcher<IdentifierMatcher> identifier(Parser* parser) {
	Matcher<IdentifierMatcher> ret = { {}, parser };
	return ret;
}

/// Match /* {any}0-* */
struct CommentMatcher
{
	bool match(Parser* parser);
};

inline Matcher<CommentMatcher> comment(Parser* parser) {
	Matcher<CommentMatcher> ret = { {}, parser };
	return ret;
}

/// Skippable things are white space and comment
struct SkippableMatcher
{
	bool match(Parser* parser);
};

inline Matcher<SkippableMatcher> skip(Parser* parser) {
	Matcher<SkippableMatcher> ret = { {}, parser };
	return ret;
}

/// Match the string of a property's value
struct PropertyValueMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PropertyValueMatcher> propertyValue(Parser* parser) {
	Matcher<PropertyValueMatcher> ret = { {}, parser };
	return ret;
}

/// Match [identifier | '*'] { '#'identifier | '.'identifier }1-*
struct SimpleSelectorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<SimpleSelectorMatcher> simpleSelector(Parser* parser) {
	Matcher<SimpleSelectorMatcher> ret = { {}, parser };
	return ret;
}

/// Match simpleSelector { [ '+' | '>' ] simpleSelector }0-*
struct SelectorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<SelectorMatcher> selector(Parser* parser) {
	Matcher<SelectorMatcher> ret = { {}, parser };
	return ret;
}

/// Match all valid CSS medium type:
/// all, aural, braille, embossed, handheld, print
/// projection, scree, tty, tv
struct MediumMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediumMatcher> medium(Parser* parser) {
	Matcher<MediumMatcher> ret = { {}, parser };
	return ret;
}

/// Match propName ':' propValue ';'
struct PropertyDeclMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PropertyDeclMatcher> propertyDecl(Parser* parser) {
	Matcher<PropertyDeclMatcher> ret = { {}, parser };
	return ret;
}

/// Match selector {',' selector}0-* '{' {propertyDecl}0-* '}'
struct RuleSetMatcher
{
	bool match(Parser* parser);
};

inline Matcher<RuleSetMatcher> ruleSet(Parser* parser) {
	Matcher<RuleSetMatcher> ret = { {}, parser };
	return ret;
}

///
struct CssMatcher
{
	bool match(Parser* parser);
};

inline Matcher<CssMatcher> css(Parser* parser) {
	Matcher<CssMatcher> ret = { {}, parser };
	return ret;
}

}	// namespace Parsing

#endif	// __DOM_CSSPARSER_H__
