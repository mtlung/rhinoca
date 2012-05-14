#ifndef __roParser_h__
#define __roParser_h__

#include "../platform/roCompiler.h"

/// A really simple "Recursive descent parser" without look ahead, back tracking, and dis-ambiguity.
/// See: http://en.wikipedia.org/wiki/Recursive-descent_parser
/// TODO: May need to use wide character type

/// Grammar used in the documentation:
/// *: 0 or more
/// +: 1 or more 
/// ?: 0 or 1
/// |: separates alternatives
/// [ ]: grouping 


namespace ro {
namespace Parsing {

struct ParserResult
{
	const char* type;
	const char* begin;
	const char* end;
};

struct Parser
{
	typedef void (*ParserResultCallback)(ParserResult* result, Parser* parser);

	Parser(const char* begin, const char* end, ParserResultCallback callback=NULL, void* userdata=NULL);

	const char* begin;
	const char* end;

	ParserResultCallback callback;
	void* userdata;

	ParserResult* customResult;

	void reportError(const char* msg);
	const char* erroMessage;
};	// Parser

template<typename T>
struct Matcher
{
	bool any()
	{
		while(t.match(parser)) {}
		return true;
	}

	bool any(ParserResult* result)
	{
		const char* bk = parser->begin;
		while(t.match(parser)) {}
		if(result) {
			result->begin = bk;
			result->end = parser->begin;
			if(parser->callback) parser->callback(result, parser);
		}
		return true;
	}

	bool count(roSize min, roSize max, ParserResult* result=NULL)
	{
		const char* bk = parser->begin, *bk2;

		// Match min occurrences
		for(roSize i=0; i<min; ++i)
			if(!t.match(parser))
				goto Fail;

		// Try to continue match() until the max occurrence reach
		for(roSize i=min; i<max; ++i)
			if(!t.match(parser))
				break;

		// If there still match, we fail
		bk2 = parser->begin;
		if(!t.match(parser)) {
			parser->begin = bk2;
			if(result) {
				result->begin = bk;
				result->end = parser->begin;
				if(parser->callback) parser->callback(result, parser);
			}
			return true;
		}

	Fail:
		parser->begin = bk;
		ParserResult ret = { NULL };
		if(result) *result = ret;
		return false;
	}

	bool atLeastOnce(ParserResult* result=NULL)
	{
		return count(1, roSize(-1), result);
	}

	bool atMostOnce(ParserResult* result=NULL)
	{
		return count(0, 1, result);
	}

	bool once()
	{
		const char* bk = parser->begin;

		ParserResult customResult = { NULL };
		parser->customResult = &customResult;

		if(!t.match(parser)) {
			parser->begin = bk;
			return false;
		}

		return true;
	}

	bool once(ParserResult* result)
	{
		const char* bk = parser->begin;

		ParserResult customResult = { NULL };
		parser->customResult = &customResult;

		if(!t.match(parser)) {
			parser->begin = bk;
			return false;
		}

		if(result) {
			if(customResult.type && customResult.begin)
				*result = customResult;
			else {
				result->begin = bk;
				result->end = parser->begin;
			}
			if(parser->callback) parser->callback(result, parser);
		}

		return true;
	}

	T t;
	Parser* parser;
};	// Matcher

/// Match single character
struct CharMatcher
{
	bool match(Parser* parser)
	{
		int b = (*parser->begin == c);
		parser->begin += b;
		return b > 0;
	}

	char c;
};

inline Matcher<CharMatcher> character(Parser* parser, char v) {
	Matcher<CharMatcher> ret = { { v }, parser };
	return ret;
}

/// Always match and didn't consume any character
/// Useful for generating result explicitly
struct DummyMatcher
{
	bool match(Parser* parser) { return true; }
};

inline Matcher<DummyMatcher> empty(Parser* parser) {
	Matcher<DummyMatcher> ret = { {}, parser };
	return ret;
}

/// Match ' ', '\t', '\r', '\n'
struct WhiteSpaceMatcher
{
	bool match(Parser* parser);
};

inline Matcher<WhiteSpaceMatcher> whiteSpace(Parser* parser) {
	Matcher<WhiteSpaceMatcher> ret = { {}, parser };
	return ret;
}

/// Match a specific string
struct StringMatcher
{
	bool match(Parser* parser);
	const char* str;
};

inline Matcher<StringMatcher> string(Parser* parser, const char* str) {
	Matcher<StringMatcher> ret = { { str }, parser };
	return ret;
}

/// Match digit numbers only, +-. will not be matched
struct DigitMatcher
{
	bool match(Parser* parser);
};

inline Matcher<DigitMatcher> digit(Parser* parser) {
	Matcher<DigitMatcher> ret = { {}, parser };
	return ret;
}

/// Match single quoted string with character escape support, eg: "any\'escaped\'"
struct QuotedStringMatcher
{
	bool match(Parser* parser);
};

inline Matcher<QuotedStringMatcher> quotedString(Parser* parser) {
	Matcher<QuotedStringMatcher> ret = { {}, parser };
	return ret;
}

/// Match double quoted string with character escape support, eg: "any\"escaped\""
struct DoubleQuotedStringMatcher
{
	bool match(Parser* parser);
};

inline Matcher<DoubleQuotedStringMatcher> doubleQuotedString(Parser* parser) {
	Matcher<DoubleQuotedStringMatcher> ret = { {}, parser };
	return ret;
}

/// Match an identifier in the C language, may applicable to other lang/format as well
/// [a-z_]+ [a-z0-9_]*
struct CIdentifierStringMatcher
{
	bool match(Parser* parser);
};

inline Matcher<CIdentifierStringMatcher> cIdentifierString(Parser* parser) {
	Matcher<CIdentifierStringMatcher> ret = { {}, parser };
	return ret;
}

struct AnyCharExceptMatcher
{
	bool match(Parser* parser);
	const char* except;
};

inline Matcher<AnyCharExceptMatcher> anyCharExcept(Parser* parser, const char* except) {
	Matcher<AnyCharExceptMatcher> ret = { { except }, parser };
	return ret;
}

}	// namespace Parsing
}	// namespace ro

#endif	// __roParser_h__
