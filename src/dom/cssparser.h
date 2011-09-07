#ifndef __DOM_CSSPARSER_H__
#define __DOM_CSSPARSER_H__

#include "../parser.h"

namespace Parsing {

/// CSS selector syntax: http://www.w3.org/TR/CSS2/selector.html#selector-syntax
/// W3C rough  http://www.w3.org/TR/1998/REC-CSS2-19980512/syndata.html
/// W3C detail http://www.w3.org/TR/1998/REC-CSS2-19980512/grammar.html
/// See http://www.codeproject.com/KB/recipes/CSSParser.aspx
/// http://stackoverflow.com/questions/4656975/use-css-selectors-to-collect-html-elements-from-a-streaming-parser-e-g-sax-stre

/// Grammar:
/// *: 0 or more
/// +: 1 or more 
/// ?: 0 or 1
/// |: separates alternatives
/// [ ]: grouping 

/// Match the top level CSS document
/// [S|CDO|CDC]* [ import [S|CDO|CDC]* ]*
/// [ [ ruleset | media | page | font_face ] [S|CDO|CDC]* ]*
struct CssMatcher
{
	bool match(Parser* parser);
};

inline Matcher<CssMatcher> css(Parser* parser) {
	Matcher<CssMatcher> ret = { {}, parser };
	return ret;
}

/// '@media' S* mediaQueryList '{' S* ruleset* '}' S*
struct MediaMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaMatcher> media(Parser* parser) {
	Matcher<MediaMatcher> ret = { {}, parser };
	return ret;
}

/// CSS3 media query: http://www.w3.org/TR/css3-mediaqueries
/// S* [mediaQuery [ ',' S* mediaQuery ]* ]?
struct MediaQueryListMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaQueryListMatcher> mediaQueryList(Parser* parser) {
	Matcher<MediaQueryListMatcher> ret = { {}, parser };
	return ret;
}

/// ['ONLY' | 'NOT']? S* media_type S* [ 'AND' S* expression ]* | expression [ 'AND' S* expression ]*
struct MediaQueryMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaQueryMatcher> mediaQuery(Parser* parser) {
	Matcher<MediaQueryMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT
struct MediaTypeMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaTypeMatcher> mediaType(Parser* parser) {
	Matcher<MediaTypeMatcher> ret = { {}, parser };
	return ret;
}

/// '(' S* mediaFeature S* [ ':' S* expr ]? ')' S*
struct MediaExpressionMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaExpressionMatcher> mediaExpression(Parser* parser) {
	Matcher<MediaExpressionMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT
struct MediaFeatureMatcher
{
	bool match(Parser* parser);
};

inline Matcher<MediaFeatureMatcher> mediaFeature(Parser* parser) {
	Matcher<MediaFeatureMatcher> ret = { {}, parser };
	return ret;
}

/// '/' S* | ',' S* | empty
struct OperatorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<OperatorMatcher> operatar(Parser* parser) {
	Matcher<OperatorMatcher> ret = { {}, parser };
	return ret;
}

/// '+' S* | '>' S* | empty
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

/// expr prio?
struct PropertyValueMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PropertyValueMatcher> propertyValue(Parser* parser) {
	Matcher<PropertyValueMatcher> ret = { {}, parser };
	return ret;
}

/// Match selector {',' selector}0-* '{' {declaration}0-* '}'
struct RuleSetMatcher
{
	bool match(Parser* parser);
};

inline Matcher<RuleSetMatcher> ruleSet(Parser* parser) {
	Matcher<RuleSetMatcher> ret = { {}, parser };
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

/// Match [identifier | '*'] { '#'identifier | '.'identifier }1-*
struct SimpleSelectorMatcher
{
	bool match(Parser* parser);
};

inline Matcher<SimpleSelectorMatcher> simpleSelector(Parser* parser) {
	Matcher<SimpleSelectorMatcher> ret = { {}, parser };
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

/// '[' S* IDENT S* [ [ '=' | INCLUDES | DASHMATCH ] S* [ IDENT | STRING ] S* ]? ']'
struct AttribMatcher
{
	bool match(Parser* parser);
};

inline Matcher<AttribMatcher> attrib(Parser* parser) {
	Matcher<AttribMatcher> ret = { {}, parser };
	return ret;
}

/// ':' [ IDENT S* '(' S* IDENT S* ')' ] | IDENT
struct PseudoMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PseudoMatcher> pseudo(Parser* parser) {
	Matcher<PseudoMatcher> ret = { {}, parser };
	return ret;
}

/// property ':' S* expr prio? | empty
struct DeclarationMatcher
{
	bool match(Parser* parser);
};

inline Matcher<DeclarationMatcher> declaration(Parser* parser) {
	Matcher<DeclarationMatcher> ret = { {}, parser };
	return ret;
}

/// Match ever property declaration enclosed by '{' and '}'
struct DeclarationsMatcher
{
	bool match(Parser* parser);
	bool includeCurryBracket;
};

inline Matcher<DeclarationsMatcher> declarations(Parser* parser, bool includeCurryBracket) {
	Matcher<DeclarationsMatcher> ret = { { includeCurryBracket }, parser };
	return ret;
}

/// ! S* important S*
struct PrioMatcher
{
	bool match(Parser* parser);
};

inline Matcher<PrioMatcher> prio(Parser* parser) {
	Matcher<PrioMatcher> ret = { {}, parser };
	return ret;
}

/// term [ operator term ]*
struct ExprMatcher
{
	bool match(Parser* parser);
};

inline Matcher<ExprMatcher> expr(Parser* parser) {
	Matcher<ExprMatcher> ret = { {}, parser };
	return ret;
}

/// unary_operator?
/// [ NUMBER S* | PERCENTAGE S* | LENGTH S* | EMS S* | EXS S* | ANGLE S* | TIME S* | FREQ S* | function ]
/// | STRING S* | IDENT S* | URI S* | RGB S* | UNICODERANGE S* | hexcolor
struct TermMatcher
{
	bool match(Parser* parser);
};

inline Matcher<TermMatcher> term(Parser* parser) {
	Matcher<TermMatcher> ret = { {}, parser };
	return ret;
}

/// IDENT S* '(' S* expr ')' S*
struct FunctionMatcher
{
	bool match(Parser* parser);
};

inline Matcher<FunctionMatcher> function(Parser* parser) {
	Matcher<FunctionMatcher> ret = { {}, parser };
	return ret;
}

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

/// Match url('http://abc.com') | url("http://abc.com") | url(http://abc.com)
struct UrlMatcher
{
	bool match(Parser* parser);
};

inline Matcher<UrlMatcher> url(Parser* parser) {
	Matcher<UrlMatcher> ret = { {}, parser };
	return ret;
}

/// NUMBER S* ( 'px' | 'cm' | 'pt' | 'deg' | 'rad' | 'grad' | 'ms' | 's' | 'hz' | 'khz' | '%' )
struct UnitMatcher
{
	bool match(Parser* parser);
};

inline Matcher<UnitMatcher> unit(Parser* parser) {
	Matcher<UnitMatcher> ret = { {}, parser };
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

/// 'none' | <transform-function> [ <transform-function> ]* 
struct TransformMatcher
{
	bool match(Parser* parser);
};

inline Matcher<TransformMatcher> transform(Parser* parser) {
	Matcher<TransformMatcher> ret = { {}, parser };
	return ret;
}

}	// namespace Parsing

#endif	// __DOM_CSSPARSER_H__
