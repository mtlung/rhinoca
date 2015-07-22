#ifndef __roLexer_h__
#define __roLexer_h__

#include "../base/roArray.h"
#include "../base/roLinkList.h"
#include "../base/roMap.h"
#include "../base/roRegex.h"
#include "../base/roString.h"

namespace ro {

struct Lexer
{
	struct LineInfo {
		LineInfo() { l1 = l2 = c1 = c2 = p1 = p2 = 0; }
		roSize l1, l2;	// Line begin/end
		roSize c1, c2;	// Column begin/end
		roSize p1, p2;	// Position from beginning of string
	};

	struct Token {
		RangedString token;
		RangedString value;
		Lexer::LineInfo lineInfo;
	};

	typedef	bool (*MatchFunc)(RangedString& inout, void* userData);

	roStatus registerStrRule(const char* strToMatch, bool isFragment=false);						// Rule name is the same as the string to match
	roStatus registerStrRule(const char* ruleName, const char* strToMatch, bool isFragment=false);	// Simple character to character matching
	roStatus registerRegexRule(const char* ruleName, const char* regex, const char* option="", bool isFragment=false);		// Regex grammar
	roStatus registerCustomRule(const char* ruleName, MatchFunc matchFunc, void* userData=NULL, bool isFragment=false);	// Matching with user function

	typedef	roStatus (*MatchingEndTokenFunc)(const RangedString& source, const Token& currentToken, RangedString& output, void* userData);
	roStatus registerMatchingEndToken(MatchingEndTokenFunc matchingEndTokenFunc, void* userData=NULL);

	roStatus beginParse(const RangedString& source);	// Please make sure source string not deleted before endParse()
	roStatus nextToken(Token& token);
	roStatus seekToMatchingEndToken();	// Find the matched end position of the current token, for instance () {} [] etc...
	roStatus endParse();

	void pushState();		// Save current state on stack
	void restoreState();	// Restore the saved state
	void discardState();	// Discard the saved state

// Private:
	struct IRule : public MapNode<String, IRule> {
		virtual ~IRule() {}
		virtual bool match(RangedString& inout) = 0;

		bool isFragment;	// Fragment can compose into rule but won't contribute to the generated token directly
		struct OrderedListNode : public ro::ListNode<IRule::OrderedListNode> {
			void destroyThis() override { removeThis(); }
		} orderedListNode;
	};	// IRule

	Map<IRule> _rules;
	LinkList<IRule::OrderedListNode> _ruleOrderedList;	// For ordered iteration

	MatchingEndTokenFunc	_matchingEndTokenFunc;
	void*					_matchingEndTokenUserData;

	Array<RangedString> _stateStack;
};	// Lexer

struct TokenStream
{
	typedef Lexer::Token Token;

	roStatus	beginParse(const RangedString& source);	// Please make sure source string not deleted before endParse()
	roStatus	consumeToken(roSize count=1);
	bool		consumeToken(const RangedString& token);
	roStatus	currentToken(Lexer::Token& token);
	roStatus	lookAhead(roSize offset, Lexer::Token& token);
	roStatus	seekToMatchingEndToken();
	roStatus	endParse();

	void pushState();		// Save current state on stack
	void restoreState();	// Restore the saved state
	void discardState();	// Discard the saved state

	struct StateScoped : NonCopyable {
		StateScoped(TokenStream& tks);
		~StateScoped();
		void consume();
		TokenStream& tks;
		bool consumed;
	};

	Array<String> tokenToIgnore;

	Lexer _lexer;
	Array<Lexer::Token> _lookAheadBuf;
	Array<roSize> _tokenPosStack;
};	// TokenStream

}	// namespace ro

#endif	// __roLexer_h__
