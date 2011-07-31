#include "pch.h"
#include "parser.h"

#include <string.h>
#include <stdio.h>

namespace Parsing {

bool WhiteSpaceMatcher::match(Parser* parser)
{
	char c = *parser->begin;
	char chars[] = { ' ', '\t', '\r', '\n' };

	for(unsigned i=0; i<sizeof(c)/sizeof(char); ++i) {
		if(c != chars[i])
			continue;

		parser->begin++;
		return true;
	}

	return false;
}

bool StringMatcher::match(Parser* parser)
{
	unsigned len = strlen(str);

	if(parser->end - parser->begin < (int)len)
		return false;

	int ret = strncmp(parser->begin, str, len);
	if(ret == 0) {
		parser->begin += len;
		return true;
	}
	return false;
}

bool DigitMatcher::match(Parser* parser)
{
	char c = *parser->begin;

	if(c < '0' || c > '9')
		return false;

	parser->begin++;
	return true;
}

bool QuotedStringMatcher::match(Parser* parser)
{
	if(*parser->begin != '\'')
		return false;

	parser->result.begin = parser->begin + 1;

	char lastChar = *parser->begin;
	for(char* p=parser->begin+1; p<parser->end; ++p)
	{
		if(*p == '\'' && lastChar != '\\') {
			parser->begin = p + 1;
			parser->result.end = p;
			return true;
		}
		lastChar = *p;
	}

	parser->begin = parser->end;
	return false;
}

bool DoubleQuotedStringMatcher::match(Parser* parser)
{
	if(*parser->begin != '"')
		return false;

	parser->result.begin = parser->begin + 1;

	char lastChar = *parser->begin;
	for(char* p=parser->begin+1; p<parser->end; ++p)
	{
		if(*p == '"' && lastChar != '\\') {
			parser->begin = p + 1;
			parser->result.end = p;
			return true;
		}
		lastChar = *p;
	}

	parser->begin = parser->end;
	return false;
}

bool AnyCharExceptMatcher::match(Parser* parser)
{
	const char* ret = strchr(except, *parser->begin);

	if(ret == NULL) {	// No exception found
		++parser->begin;
		return true;
	}

	return false;
}

}	// namespace Parsing
