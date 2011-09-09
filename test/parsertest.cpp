#include "pch.h"
#include "../src/parser.h"
#include "../src/rhstring.h"
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
		StringLowerCaseHash typeHash(result->type, 0);

		if(	typeHash == StringHash("selectors") ||
			typeHash == StringHash("decls") ||
			typeHash == StringHash("ruleset") ||
			typeHash == StringHash("combinator")
		)
			return;

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

	{	// Test again with spacing
		char data[] = " * { a : b} ";
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

	{	// Test again with spacing
		char data[] = " * E { a : b; }";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:*;selector:E;propName:a;propVal:b;", str);
	}

	{	// Try to confuse the parser with some comments
		char data[] = "/*!*/*/*xxx*//*yyy*/{/**/a:b;/* */}";
		Parser parser(data, data + sizeof(data), parserCallback, &str);

		str.clear();
		CHECK(css(&parser).once());
		CHECK_EQUAL("selector:*;propName:a;propVal:b;", str);
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

class CSSBackgroundPositionParserTest
{
public:
	static void parserCallback(ParserResult* result, Parser* parser)
	{
		std::string& str = *reinterpret_cast<std::string*>(parser->userdata);
		str += result->type;

		str += ":";
		str.append(result->begin, result->end);
		str += ";";
	}

	std::string str;
};

TEST_FIXTURE(CSSBackgroundPositionParserTest, basic)
{
	char data[] = " -10px, 20 ; ";
	Parser parser(data, data + sizeof(data), parserCallback, &str);

	str.clear();
	CHECK(backgroundPosition(&parser).once());
	CHECK_EQUAL("value:-10;unit:px;value:20;unit:;end:;", str);
}

class CSSTransformParserTest
{
public:
	static void parserCallback(ParserResult* result, Parser* parser)
	{
		std::string& str = *reinterpret_cast<std::string*>(parser->userdata);
		str += result->type;

		str += ":";
		str.append(result->begin, result->end);
		str += ";";
	}

	std::string str;
};

TEST_FIXTURE(CSSTransformParserTest, none)
{
	char data[] = " none ; ";
	Parser parser(data, data + sizeof(data), parserCallback, &str);

	str.clear();
	CHECK(transform(&parser).any());
	CHECK_EQUAL("none:none;end:;", str);
}

TEST_FIXTURE(CSSTransformParserTest, basic)
{
	char data[] = " skewx ( 10 deg ) translatex ( -10 ) translatey ( 150 px ) ; ";
	Parser parser(data, data + sizeof(data), parserCallback, &str);

	str.clear();
	CHECK(transform(&parser).any());
	CHECK_EQUAL("name:skewx;value:10;unit:deg;end:;name:translatex;value:-10;unit:;end:;name:translatey;value:150;unit:px;end:;", str);
}

TEST_FIXTURE(CSSTransformParserTest, translate)
{
	char data[] = "translate( 10, 20 px );";
	Parser parser(data, data + sizeof(data), parserCallback, &str);

	str.clear();
	CHECK(transform(&parser).any());
	CHECK_EQUAL("name:translate;value:10;unit:;value:20;unit:px;end:;", str);
}
