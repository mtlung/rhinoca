#include "pch.h"
#include "roParser.h"
#include "roStringUtility.h"

namespace ro {
namespace Parsing {

Parser::Parser(const char* b, const char* e, ParserResultCallback c, void* u)
	: begin(b), end(e)
	, callback(c), userdata(u)
	, erroMessage("")
{
}

void Parser::reportError(const char* msg)
{
	erroMessage = msg;

	if(callback) {
		ParserResult error = { "error", begin, end };
		callback(&error, this);
	}
}

bool WhiteSpaceMatcher::match(Parser* p)
{
	char c = *p->begin;
	char chars[] = { ' ', '\t', '\r', '\n' };

	for(roSize i=0; i<sizeof(chars)/sizeof(char); ++i) {
		if(c != chars[i])
			continue;

		p->begin++;
		return true;
	}

	return false;
}

bool StringMatcher::match(Parser* p)
{
	roSize len = roStrLen(str);

	if(p->end - p->begin < (int)len)
		return false;

	int ret = roStrnCmp(p->begin, str, len);
	if(ret == 0) {
		p->begin += len;
		return true;
	}
	return false;
}

bool DigitMatcher::match(Parser* p)
{
	char c = *p->begin;

	if(c < '0' || c > '9')
		return false;

	p->begin++;
	return true;
}

bool NumberMatcher::match(Parser* p)
{
	const char* c=p->begin;

	for(;; ++c) if(roIsDigit(*c)) break;
	if(*c == '.') ++c;
	for(;; ++c) if(roIsDigit(*c)) break;

	if(c > p->begin) p->begin = c;
	return c > p->begin;
}

bool QuotedStringMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	if(*p->begin != '\'')
		return false;

	result.begin = p->begin + 1;

	char lastChar = *p->begin;
	for(const char* c=p->begin+1; c<p->end; ++c)
	{
		if(*c == '\'' && lastChar != '\\') {
			p->begin = c + 1;
			result.end = c;
			return true;
		}
		lastChar = *c;
	}

	p->begin = p->end;
	return false;
}

bool DoubleQuotedStringMatcher::match(Parser* p)
{
	ParserResult& result = *p->customResult;

	if(*p->begin != '"')
		return false;

	result.begin = p->begin + 1;

	char lastChar = *p->begin;
	for(const char* c=p->begin+1; c<p->end; ++c)
	{
		if(*c == '"' && lastChar != '\\') {
			p->begin = c + 1;
			result.end = c;
			return true;
		}
		lastChar = *c;
	}

	p->begin = p->end;
	return false;
}

bool CIdentifierStringMatcher::match(Parser* p)
{
	const char* c = p->begin;
	if( !roIsAlpha(*c) && *c != '_' )
		return false;

	for(; (++c) < p->end;)
	{
		if( !roIsAlpha(*c) && !roIsDigit(*c) && *c != '_' )
			break;
	}

	p->begin = c;
	return true;
}

bool AnyCharExceptMatcher::match(Parser* p)
{
	const char* ret = roStrChr(except, *p->begin);

	if(ret == NULL) {	// No exception found
		++p->begin;
		return true;
	}

	return false;
}

}	// namespace Parsing
}	// namespace ro
