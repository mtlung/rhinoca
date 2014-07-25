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
	lexer.registerRule("Comment", LocalScope::matchComment, NULL);

	lexer.registerRule("WhiteSpace",
		"[ \t\n\r]+");

	lexer.registerRule("StringLiteral",
		"L?\"(\\.|[^\"])*\"");

	lexer.registerRule("FloatExponent",
		"[eE][+-]?[0-9]+", true);

	lexer.registerRule("FloatLiteral",
		"([0-9]+\\.[0-9]*){FloatExponent}?|"
		"([0-9]+){FloatExponent}?|"
		"\\.[0-9]+{FloatExponent}?"
	);

	lexer.registerRule("Comment",
		"//[^\r\n]*");

	lexer.registerRule("Identifier",
		"[a-zA-Z_][a-zA-Z0-9_]*");

	lexer.registerRule("=",		"=");
	lexer.registerRule("==",	"==");
	lexer.registerRule("(",		"\\(");
	lexer.registerRule(")",		"\\)");

	// Key words
	lexer.registerRule("int",			"int");
	lexer.registerRule("double",		"double");

	String source(
		"/**//*multiline \ncomment*/ double a=4e2 if(a==0.123) b=4.7e2 \"hello\" // Comment"
	);

	CHECK(lexer.beginParse(source));

	RangedString token, val;
	Lexer::LineInfo lineInfo;
	while(lexer.nextToken(token, val, lineInfo))
	{
		token = token;
	}

	CHECK(lexer.endParse());
}
