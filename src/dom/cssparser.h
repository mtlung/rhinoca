#ifndef __DOM_CSSPARSER_H__
#define __DOM_CSSPARSER_H__

#include "../parser.h"

namespace Parsing {

/// CSS selector syntax: http://www.w3.org/TR/CSS2/selector.html#selector-syntax
/// W3C rough  http://www.w3.org/TR/1998/REC-CSS2-19980512/syndata.html
/// W3C detail http://www.w3.org/TR/1998/REC-CSS2-19980512/grammar.html
/// See http://www.codeproject.com/KB/recipes/CSSParser.aspx
/// http://stackoverflow.com/questions/4656975/use-css-selectors-to-collect-html-elements-from-a-streaming-parser-e-g-sax-stre

/// EBNF language used inside the comment:
/// Alternatives: |, group by ()
/// Repetitions: {}0-* {}1-*
/// Options: []

/// [0-9]+|[0-9]*"."[0-9]+
struct NumberMatcher
{
	bool match(Parser* parser);
};

inline Matcher<NumberMatcher> number(Parser* parser) {
	Matcher<NumberMatcher> ret = { {}, parser };
	return ret;
}

/// {[a-z0-9-]|{nonascii}|{escape}}+
struct NameMatcher
{
	bool match(Parser* parser);
};

inline Matcher<NameMatcher> name(Parser* parser) {
	Matcher<NameMatcher> ret = { {}, parser };
	return ret;
}

/// [a-z]|{nonascii}|{escape} {name}
struct IdentMatcher
{
	bool match(Parser* parser);
};

inline Matcher<IdentMatcher> ident(Parser* parser) {
	Matcher<IdentMatcher> ret = { {}, parser };
	return ret;
}

/// '#'{name}
struct HashMatcher
{
	bool match(Parser* parser);
};

inline Matcher<HashMatcher> hash(Parser* parser) {
	Matcher<HashMatcher> ret = { {}, parser };
	return ret;
}

/// '@media' S* medium [ ',' S* medium ]* '{' S* ruleset* '}' S*
struct MediaMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaMatcher> media(Parser* parser) {
	Matcher<MediaMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT S*
struct MediumMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediumMatcher> medium(Parser* parser) {
	Matcher<MediumMatcher> ret = { {}, parser };
	return ret;
}

/// '/' S* | ',' S*
struct OperatorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<OperatorMatcher> operatar(Parser* parser) {
	Matcher<OperatorMatcher> ret = { {}, parser };
	return ret;
}

/// '+' S* | '>' S*
struct CombinatorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<CombinatorMatcher> combinator(Parser* parser) {
	Matcher<CombinatorMatcher> ret = { {}, parser };
	return ret;
}

/// '-' | '+'
struct UnaryOperatorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<UnaryOperatorMatcher> unaryOperator(Parser* parser) {
	Matcher<UnaryOperatorMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT S*
struct PropertyMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PropertyMatcher> property(Parser* parser) {
	Matcher<PropertyMatcher> ret = { {}, parser };
	return ret;
}

/// '.' IDENT
struct ClassMatcher
{
	bool match(Parser* parser);
};

inline Matcher<ClassMatcher> klass(Parser* parser) {
	Matcher<ClassMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT | '*'
struct ElementNameMatcher
{
	bool match(Parser* parser);
};

inline Matcher<ElementNameMatcher> elementName(Parser* parser) {
	Matcher<ElementNameMatcher> ret = { {}, parser };
	return ret;
}

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

///
struct AnyMatcher
{
	bool match(Parser* parser);
};

inline Matcher<AnyMatcher> any(Parser* parser) {
	Matcher<AnyMatcher> ret = { {}, parser };
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

/// Match the whole selector string for a rule set
struct SelectorsMatcher
{
	bool match(Parser* parser);
};

inline Matcher<SelectorsMatcher> selectors(Parser* parser) {
	Matcher<SelectorsMatcher> ret = { {}, parser };
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

/// Match ever property declaration enclosed by '{' and '}'
struct PropertyDeclsMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PropertyDeclsMatcher> propertyDecls(Parser* parser) {
	Matcher<PropertyDeclsMatcher> ret = { {}, parser };
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

/// Match the top level CSS document
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
