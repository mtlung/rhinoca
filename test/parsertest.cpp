#include "pch.h"
#include "../src/parser.h"

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
