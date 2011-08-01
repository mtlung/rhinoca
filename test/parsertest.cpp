#include "pch.h"
#include "../src/parser.h"
#include "../src/dom/cssparser.h"

using namespace Parsing;

class ParserTest {};

TEST_FIXTURE(ParserTest, character)
{
	char data[] = "abcdef";
	Parser parser = { data, data + sizeof(data) };

	CHECK(character(&parser, 'a').once());
	CHECK(*parser.begin == 'b');

	CHECK(!character(&parser, 'g').once());
	CHECK(*parser.begin == 'b');

	CHECK(character(&parser, 'g').any());
	CHECK(*parser.begin == 'b');
}

TEST_FIXTURE(ParserTest, digit)
{
	char data[] = "0123 abc";
	Parser parser = { data, data + sizeof(data) };

	CHECK(digit(&parser).once());
	CHECK(*parser.begin == '1');

	CHECK(digit(&parser).any());
	CHECK(*parser.begin == ' ');
}

TEST_FIXTURE(ParserTest, string)
{
	char data[] = "Hello world!";
	Parser parser = { data, data + sizeof(data) };

	CHECK(string(&parser, "Hello").once());
	CHECK(*parser.begin == ' ');

	CHECK(!string(&parser, "Foo").once());
}

static void parserCallback(ParserResult* result)
{
	if(result->type)
		printf("%s, ", result->type);

	for(const char* i=result->begin; i<result->end; ++i)
		putchar(*i);
	printf("\n");
}

TEST_FIXTURE(ParserTest, css)
{
	char data[] =
		".button { background-image : url ( \"http://mtlung.blogspot.com\" ) ; background-repeat : no-repeat ; }\n"
		"#buttonLeft , abc def { left : 0; background-position : 0 , 0 ; }";

	Parser parser = { data, data + sizeof(data), parserCallback };

	CHECK(css(&parser).once());
}
