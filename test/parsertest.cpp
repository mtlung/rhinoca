#include "pch.h"
#include "../src/parser.h"
#include "../src/dom/cssparser.h"

using namespace Parsing;

class ParserTest {};

TEST_FIXTURE(ParserTest, character)
{
	char data[] = "abcdef";
	Parser parser(data, data + sizeof(data));

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
	Parser parser(data, data + sizeof(data));

	CHECK(digit(&parser).once());
	CHECK(*parser.begin == '1');

	CHECK(digit(&parser).any());
	CHECK(*parser.begin == ' ');
}

TEST_FIXTURE(ParserTest, string)
{
	char data[] = "Hello world!";
	Parser parser(data, data + sizeof(data));

	CHECK(string(&parser, "Hello").once());
	CHECK(*parser.begin == ' ');

	CHECK(!string(&parser, "Foo").once());
}

class CSSSelectorParserTest
{
public:
	static void parserCallback(ParserResult* result, Parser* parser)
	{
		std::string& str = *reinterpret_cast<std::string*>(parser->userdata);
		str += result->type;

		if(strcmp(result->type, "error:") == 0) {
			str += parser->erroMessage;
			str += "near";
		}

		str += ":";
		str.append(result->begin, result->end);
		str += ";";
	}

	std::string str;
};

TEST_FIXTURE(CSSSelectorParserTest, universal)
{
	{	char data[] = "*{a:b}";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:*;propName:a;propVal:b;", str);
	}

	{	char data[] = "*E{a:b;}";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:*;selector:E;propName:a;propVal:b;", str);
	}
}

TEST_FIXTURE(CSSSelectorParserTest, group)
{
	{	char data[] = "h1, h2, h3 {a:b}";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:h1;sGroup:,;selector:h2;sGroup:,;selector:h3;propName:a;propVal:b;", str);
	}

	{	char data[] = "*E, *F{a:b;}";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:*;selector:E;sGroup:,;selector:*;selector:F;propName:a;propVal:b;", str);
	}
}
