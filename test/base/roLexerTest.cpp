#include "pch.h"
#include "../../roar/base/roLexer.h"
#include "../../roar/base/roRegex.h"

using namespace ro;

class LexerTest {};

TEST_FIXTURE(LexerTest, basic)
{
	Lexer lexer;

	struct LocalScope {
		static bool matchComment(RangedString& inout, void* userData)
		{
			RangedString org = inout;
			for(roSize openCount = 0; inout.size() >= 2; ) {
				bool foundOpen  = (inout[0] == '/' && inout[1] == '*');
				bool foundClose = (inout[0] == '*' && inout[1] == '/');
				inout.begin += (foundOpen || foundClose) ? 2 : 1;

				openCount += foundOpen ? 1 : 0;
				openCount -= foundClose ? 1 : 0;

				if(openCount == 0)
					break;
			}
			
			const char min[] = "/**/";
			(void)min;	// Suppress warning
			return inout.begin - org.begin >= (sizeof(min) - 1);
		}
	};
	lexer.registerCustomRule("MLComment", LocalScope::matchComment, NULL);

	lexer.registerRegexRule("Comment",
		"{SLComment}|{MLComment}");

	lexer.registerRegexRule("WhiteSpace",
		"[ \t\n\r]+");

	lexer.registerRegexRule("StringLiteral",
		"L?\"(\\.|[^\"])*\"");

	lexer.registerRegexRule("FloatExponent",
		"[eE][+-]?[0-9]+", "", true);

	lexer.registerRegexRule("FloatLiteral",
		"([0-9]+\\.[0-9]*){FloatExponent}?|"
		"([0-9]+){FloatExponent}?|"
		"\\.[0-9]+{FloatExponent}?"
	);

	lexer.registerRegexRule("SLComment",
		"//[^\r\n]*");

	lexer.registerRegexRule("Identifier",
		"[a-zA-Z_][a-zA-Z0-9_]*");

	lexer.registerRegexRule("=",	"=");
	lexer.registerRegexRule("==",	"==");
	lexer.registerRegexRule("(",	"\\(");
	lexer.registerRegexRule(")",	"\\)");

	// https://developer.apple.com/library/prerelease/ios/documentation/Swift/Conceptual/Swift_Programming_Language/LexicalStructure.html
	// https://developer.apple.com/library/prerelease/ios/documentation/Swift/Conceptual/Swift_Programming_Language/zzSummaryOfTheGrammar.html
	// Keywords used in declarations
	lexer.registerRegexRule("class",		"class");
	lexer.registerRegexRule("deinit",		"deinit");
	lexer.registerRegexRule("enum",			"enum");
	lexer.registerRegexRule("extension",	"extension");
	lexer.registerRegexRule("func",			"func");
	lexer.registerRegexRule("import",		"import");
	lexer.registerRegexRule("init",			"init");
	lexer.registerRegexRule("let",			"let");
	lexer.registerRegexRule("protocol",		"protocol");
	lexer.registerRegexRule("static",		"static");
	lexer.registerRegexRule("struct",		"struct");
	lexer.registerRegexRule("subscript",	"subscript");
	lexer.registerRegexRule("typealias",	"typealias");
	lexer.registerRegexRule("var",			"var");

	// Keywords used in statements
	lexer.registerRegexRule("break",		"break");
	lexer.registerRegexRule("case",			"case");
	lexer.registerRegexRule("continue",		"continue");
	lexer.registerRegexRule("default",		"default");
	lexer.registerRegexRule("do",			"do");
	lexer.registerRegexRule("else",			"else");
	lexer.registerRegexRule("fallthrough",	"fallthrough");
	lexer.registerRegexRule("if",			"if");
	lexer.registerRegexRule("in",			"in");
	lexer.registerRegexRule("for",			"for");
	lexer.registerRegexRule("return",		"return");
	lexer.registerRegexRule("switch",		"switch");
	lexer.registerRegexRule("where",		"where");
	lexer.registerRegexRule("while",		"while");

	// Keywords used in expressions and types
	lexer.registerRegexRule("as",			"as");
	lexer.registerRegexRule("dynamicType",	"dynamicType");
	lexer.registerRegexRule("is",			"is");
	lexer.registerRegexRule("new",			"new");
	lexer.registerRegexRule("super",		"super");
	lexer.registerRegexRule("self",			"self");
	lexer.registerRegexRule("Self",			"Self");
	lexer.registerRegexRule("Type",			"Type");
	lexer.registerRegexRule("__COLUMN__",	"__COLUMN__");
	lexer.registerRegexRule("__FILE__",		"__FILE__");
	lexer.registerRegexRule("__FUNCTION__",	"__FUNCTION__");
	lexer.registerRegexRule("__LINE__",		"__LINE__");

	// Key words
	lexer.registerRegexRule("int",			"int");
	lexer.registerRegexRule("double",		"double");

	String source(
		"/**//*multiline \ncomment*/ double a=4e2 if(a==0.123) b=4.7e2 \"hello\" // Comment"
	);

	CHECK(lexer.beginParse(source));

	Lexer::Token token;
	while(lexer.nextToken(token))
	{
		token = token;
	}

	CHECK(lexer.endParse());
}

TEST_FIXTURE(LexerTest, seekToMatchingEnd)
{
	struct LocalScope {
		static bool matchMultiLineComment(RangedString& inout, void* userData)
		{
			RangedString org = inout;
			for(roSize openCount = 0; inout.size() >= 2; ) {
				bool foundOpen  = (inout[0] == '/' && inout[1] == '*');
				bool foundClose = (inout[0] == '*' && inout[1] == '/');
				inout.begin += (foundOpen || foundClose) ? 2 : 1;

				openCount += foundOpen ? 1 : 0;
				openCount -= foundClose ? 1 : 0;

				if(openCount == 0)
					break;
			}

			const char min[] = "/**/";
			(void)min;	// Suppress warning
			return inout.begin - org.begin >= (sizeof(min) - 1);
		}

		static bool matchSingleLineComment(RangedString& inout, void* userData)
		{
			return false;
		}

		static bool matchStringLiteral(RangedString& inout, void* userData)
		{
			return false;
		}

		static roStatus matchingEndToken(const RangedString& source, const Lexer::Token& currentToken, RangedString& output, void* userData)
		{
			struct Element {
				const char* open, *close;
				bool (*matchFunc)(RangedString& inout, void* userData);
			};

			const Element elements[] = {
//				{ "//",	"\n",	matchSingleLineComment },
				{ "/*",	"*/",	matchMultiLineComment },
//				{ "\"",	"\"",	matchStringLiteral },
//				{ "'",	"'",	NULL },
				{ "{",	"}",	NULL },
				{ "(",	")",	NULL },
			};

			Array<const Element*> stack;

			for(const roUtf8* b=source.begin, *e=source.end; b < e; )
			{
				const roUtf8* b2 = b;

				for(const Element& i : elements)
				{
					if(i.matchFunc && roStrCmpMinLen(b, i.open) == 0) {
						RangedString s(b, e);
						if(!(*i.matchFunc)(s, NULL))
							return roStatus::string_parsing_error;

						if(stack.isEmpty() && roStrCmpMinLen(currentToken.token.begin, i.open) == 0) {
							output =  RangedString(b, s.begin);
							return roStatus::ok;
						}

						b = s.begin;
						break;
					}

					// Account for open token
					if(roStrCmpMinLen(b, i.open) == 0) {
						stack.pushBack(&i);
						++b;
						break;
					}

					// Account for closing token
					if(roStrCmpMinLen(b, i.close) == 0) {
						if(stack.isEmpty() || stack.back() != &i)
							return roStatus::string_parsing_error;

						stack.popBack();
						if(stack.isEmpty()) {
							output =  RangedString(b, b + roStrLen(i.close));
							return roStatus::ok;
						}

						++b;
						break;
					}
				}

				if(b == b2)
					++b;
			}

			return roStatus::end_of_data;
		}
	};

	Lexer lexer;

	lexer.registerCustomRule("MLComment", LocalScope::matchMultiLineComment, NULL);
	lexer.registerMatchingEndToken(&LocalScope::matchingEndToken);
	lexer.registerStrRule("(",			"(");
	lexer.registerStrRule(")",			")");
	lexer.registerStrRule("{",			"{");
	lexer.registerStrRule("}",			"}");

	const roUtf8* testData[][2] = {
		{ "{}",						"}" },
		{ "()",						")" },
		{ "{()}",					"}" },
		{ "{({(({{}}))})}",			"}" },
		{ "{(/*comment*/)}",		"}" },
		{ "{(/*comment*/)(/**/)}",	"}" },
		{ "{",						NULL },
		{ "}",						NULL },
		{ "{)",						NULL },
		{ "{{})",					NULL },
	};

	for(roSize i=0; i<roCountof(testData); ++i) {
		CHECK(lexer.beginParse(testData[i][0]));

		if(testData[i][1]) {
			CHECK(lexer.seekToMatchingEndToken());
			Lexer::Token t;
			lexer.nextToken(t);
			CHECK_EQUAL(testData[i][1], t.token);
		}
		else
			CHECK(!lexer.seekToMatchingEndToken());

		CHECK(lexer.endParse());
	}
}
