#ifndef __PARSER_H__
#define __PARSER_H__

namespace Parsing {

struct ParserResult
{
	const char* type;
	char* begin;
	char* end;
};

class Parser
{
public:
	char* begin;
	char* end;

	typedef void (*ParserResultCallback)(ParserResult* result);
	ParserResultCallback callback;

	ParserResult result;
};	// Parser

template<typename T>
struct Matcher
{
	bool any(ParserResult* result=NULL)
	{
		char* bk = parser->begin;
		while(t.match(parser)) {}
		if(result) {
			result->begin = bk;
			result->end = parser->begin;
			parser->callback(result);
		}
		return true;
	}

	bool count(unsigned min, unsigned max, ParserResult* result=NULL)
	{
		char* bk = parser->begin;

		// Match min occurrences
		for(unsigned i=0; i<min; ++i)
			if(!t.match(parser))
				goto Fail;

		// Try to continue match() until the max occurrence reach
		for(unsigned i=min; i<max; ++i)
			if(!t.match(parser))
				break;

		// If there still match, we fail
		char* bk2 = parser->begin;
		if(!t.match(parser)) {
			parser->begin = bk2;
			if(result) {
				result->begin = bk;
				result->end = parser->begin;
				parser->callback(result);
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
		return count(1, unsigned(-1), result);
	}

	bool atMostOnce(ParserResult* result=NULL)
	{
		return count(0, 1, result);
	}

	bool once(ParserResult* result=NULL)
	{
		parser->result.begin = NULL;
		char* bk = parser->begin;

		if(!t.match(parser)) {
			parser->begin = bk;
			return false;
		}

		if(result) {
			if(parser->result.begin)
				*result = parser->result;
			else {
				result->begin = bk;
				result->end = parser->begin;
			}
			parser->callback(result);
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

#endif	// __PARSER_H__
