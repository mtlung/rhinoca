#include "pch.h"
#include "../../roar/base/roParser.h"
#include "../../roar/base/roString.h"

struct ParserTest {};

namespace ro {
namespace Parsing {

struct AttributeMatcher
{
	bool match(Parser* p) {
		ParserResult attNameResult = { "attName", NULL, NULL };
		ParserResult attValResult = { "attVal", NULL, NULL };
		return
			whiteSpace(p).any() && cIdentifierString(p).once(&attNameResult) &&
			whiteSpace(p).any() && character(p, '=').once() &&
			whiteSpace(p).any() && (doubleQuotedString(p).once(&attValResult) | quotedString(p).once(&attValResult));
	}
};

inline Matcher<AttributeMatcher> attribute(Parser* parser) {
	Matcher<AttributeMatcher> ret = { {}, parser };
	return ret;
}

/// Match anything except '<'
struct CharacterNodeMatcher
{
	bool match(Parser* p) {
		const char* c = p->begin;
		for(; c < p->end; ++c) {
			if( *c == '<' )
				break;
		}

		bool ret = p->begin != c;
		p->begin = c;
		return ret;
	}
};

inline Matcher<CharacterNodeMatcher> characterNode(Parser* parser) {
	Matcher<CharacterNodeMatcher> ret = { {}, parser };
	return ret;
}

struct ElementMatcher
{
	bool match(Parser* p);
};

inline Matcher<ElementMatcher> element(Parser* parser) {
	Matcher<ElementMatcher> ret = { {}, parser };
	return ret;
}

bool ElementMatcher::match(Parser* p)
{
	ParserResult eleBeginResult = { "eleBegin", NULL, NULL };
	ParserResult charNodedResult = { "charNode", NULL, NULL };
	ParserResult eleEndResult = { "eleEnd", NULL, NULL };

	bool ret =
		whiteSpace(p).any() && character(p, '<').once() &&
		whiteSpace(p).any() && cIdentifierString(p).once(&eleBeginResult) &&
		whiteSpace(p).any() && attribute(p).any() &&
		whiteSpace(p).any() && character(p, '>').once();
	if(!ret) return false;

	ret = (element(p).atLeastOnce() || characterNode(p).atMostOnce(&charNodedResult));
	if(!ret) return false;

	ret =
		whiteSpace(p).any() && string(p, "</").once() &&
		whiteSpace(p).any() && cIdentifierString(p).once(&eleEndResult) &&
		whiteSpace(p).any() && character(p, '>').once();

	return ret;
}

struct XmlMatcher
{
	bool match(Parser* p) {
		return element(p).atLeastOnce();
	}
};

inline Matcher<XmlMatcher> xml(Parser* parser) {
	Matcher<XmlMatcher> ret = { {}, parser };
	return ret;
}

static void xmlParserCallback(ParserResult* result, Parser* parser)
{
	result = result;
}

}	// namespace Parsing
}	// namespace ro

TEST_FIXTURE(ParserTest, xml)
{
	using namespace ro;
	using namespace ro::Parsing;

//	String xmlStr("<xml></xml>");
//	String xmlStr("<xml>abc</xml>");
	String xmlStr("<xml><e1></e2></xml>");
//	String xmlStr("<xml><ele1 att1='abc' att2=\"def\">chardata</ele></xml>");
	Parser parser(xmlStr.c_str(), xmlStr.c_str() + xmlStr.size(), xmlParserCallback, NULL);
	ro::Parsing::xml(&parser).atLeastOnce();
}
