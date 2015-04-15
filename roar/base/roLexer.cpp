#include "pch.h"
#include "roLexer.h"
#include "../base/roStringFormat.h"
#include "../base/roAlgorithm.h"

namespace ro {

roStatus Lexer::beginParse(const RangedString& source)
{
	_stateStack.clear();
	_stateStack.pushBack(source);
	return roStatus::ok;
}

roStatus Lexer::nextToken(Token& token)// RangedString& token, RangedString& val, LineInfo& lineInfo)
{
	struct Longest {
		RangedString token, value;
	};
	Array<Longest> longest;

	if(_stateStack.isEmpty())
		return roStatus::end_of_data;

	RangedString& src = _stateStack.back();

	// Try to match with all rules and proceed with the longest result
	// See http://en.wikipedia.org/wiki/Maximal_munch
	for(IRule::OrderedListNode* n = _ruleOrderedList.begin(); n != _ruleOrderedList.end(); n=n->next()) {
		IRule& r = *(roContainerof(IRule, orderedListNode, n));
		if(r.isFragment)
			continue;

		RangedString tmp(src);
		if(!r.match(tmp))
			continue;

		Longest l = { r.key(), RangedString(src.begin, tmp.begin) };
		longest.pushBack(l);
	}

	// If there were multiple results having the same length, the later registered rule have the higher priority
	roSize longestLen = 0;
	for(Longest& i : longest) {
		roSize len = i.value.size();
		if(len < longestLen)
			continue;

		longestLen = len;
		token.token = i.token;
		token.value = i.value;
	}

	// Line, position counting
	token.lineInfo.l1 = token.lineInfo.l2;
	token.lineInfo.c1 = token.lineInfo.c2;
	token.lineInfo.p1 = token.lineInfo.p2;
	for(roSize i=0; i<token.value.size(); ++i) {
		++token.lineInfo.p2;
		if(token.value[i] == '\n') {
			++token.lineInfo.l2;
			token.lineInfo.c2 = 0;
		}
		else if(token.value[i] == '\r')
			token.lineInfo.c2 = 0;
		else
			++token.lineInfo.c2;
	}

	if(longest.isEmpty())
		return roStatus::end_of_data;

	src.begin = token.value.end;
	return roStatus::ok;
}

roStatus Lexer::endParse()
{
	return roStatus::ok;
}

void Lexer::pushState()
{
	RangedString cur = _stateStack.back();
	_stateStack.pushBack(cur);
}

void Lexer::restoreState()
{
	_stateStack.popBack();
}

void Lexer::discardState()
{
	RangedString cur = _stateStack.back();
	_stateStack.popBack();
	_stateStack.back() = cur;
}

static DefaultAllocator _allocator;

struct StringRule : public Lexer::IRule {
	virtual bool match(RangedString& inout) override
	{
		roSize minSize = roMinOf2(inout.size(), str.size());
		if(0 == roStrnCmp(inout.begin, minSize, str.c_str(), str.size()))
			return inout.begin += str.size(), true;
		return false;
	}
	String str;
};	// StringRule

roStatus Lexer::registerStrRule(const char* strToMatch, bool isFragment)
{
	roStatus st;

	if(_rules.find(strToMatch))
		return roStatus::already_exist;

	AutoPtr<StringRule> rule(_allocator.newObj<StringRule>());
	rule->setKey(strToMatch);
	rule->isFragment = isFragment;
	rule->str = strToMatch;

	_ruleOrderedList.pushBack(rule->orderedListNode);
	_rules.insert(*rule.unref());

	return roStatus::ok;
}

roStatus Lexer::registerStrRule(const char* ruleName, const char* strToMatch, bool isFragment)
{
	roStatus st;

	if(_rules.find(ruleName))
		return roStatus::already_exist;

	AutoPtr<StringRule> rule(_allocator.newObj<StringRule>());
	rule->setKey(ruleName);
	rule->isFragment = isFragment;
	rule->str = strToMatch;

	_ruleOrderedList.pushBack(rule->orderedListNode);
	_rules.insert(*rule.unref());

	return roStatus::ok;
}

static bool _customMatch(RangedString& inout, void* userData)
{
	Lexer::IRule* r = reinterpret_cast<Lexer::IRule*>(userData);
	if(!r) return false;
	return r->match(inout);
}

struct RegexRule : public Lexer::IRule {
	RegexRule() : option("b") {}
	virtual bool match(RangedString& inout) override
	{
		Regex regex;
		if(!regex.match(inout, this->regex, this->matcher, option.c_str())) return false;
		inout.begin = regex.result[0].end;
		return true;
	}

	Regex::Compiled regex;
	String option;
	TinyArray<Regex::CustomMatcher, 4> matcher;
};	// RegexRule

roStatus Lexer::registerRegexRule(const char* ruleName, const char* regex, const char* option, bool isFragment)
{
	roStatus st;

	if(_rules.find(ruleName))
		return roStatus::already_exist;

	RangedString str(regex);
	AutoPtr<RegexRule> rule(_allocator.newObj<RegexRule>());
	rule->setKey(ruleName);
	rule->isFragment = isFragment;
	String regexStr;

	// Scan for {rulename}
	roSize ruleNameCount = 0;
	for(const char* c=str.begin; *c; ++c) {
		if(*c == '\\') {	// Skip escaped '\'
			st = regexStr.append(*c);
			if(!st) return st;
			++c;
			st = regexStr.append(*c);
			if(!st) return st;
			continue;
		}

		if(*c == '{') {
			Regex regex;
			RangedString result(c);
			if(regex.match(result, "\\{([A-Za-z][A-za-z0-9]*)\\}")) {
				c = regex.result[0].end - 1;

				st = strFormat(regexStr, "${}", ruleNameCount);
				++ruleNameCount;

				IRule* r = _rules.find(regex.result[1].toString().c_str());
				if(!r) return roStatus::not_found;

				Regex::CustomMatcher regCustomMatcher = { _customMatch, r };
				st = rule->matcher.pushBack(regCustomMatcher);
				if(!st) return st;
				continue;
			}
		}

		st = regexStr.append(*c);
		if(!st) return st;
	}

	st = Regex::compile(regexStr.c_str(), rule->regex);
	if(!st) return st;

	rule->option += option;
	_ruleOrderedList.pushBack(rule->orderedListNode);
	_rules.insert(*rule.unref());

	return roStatus::ok;
}

struct CustomRule : public Lexer::IRule {
	virtual bool match(RangedString& inout) override
	{
		if(!matchFunc) return false;
		return (*matchFunc)(inout, userData);
	}
	Lexer::MatchFunc matchFunc;
	void* userData;
};	// CustomRule

roStatus Lexer::registerCustomRule(const char* ruleName, MatchFunc matchFunc, void* userData, bool isFragment)
{
	roStatus st;

	if(_rules.find(ruleName))
		return roStatus::already_exist;

	AutoPtr<CustomRule> rule(_allocator.newObj<CustomRule>());
	rule->setKey(ruleName);
	rule->isFragment = isFragment;
	rule->matchFunc = matchFunc;
	rule->userData = userData;

	_ruleOrderedList.pushBack(rule->orderedListNode);
	_rules.insert(*rule.unref());

	return roStatus::ok;
}

roStatus TokenStream::beginParse(const RangedString& source)
{
	_lookAheadBuf.clear();
	roStatus st = _tokenPosStack.resize(1, 0);
	if(!st) return st;
	return _lexer.beginParse(source);
}

roStatus TokenStream::consumeToken(roSize count)
{
	roStatus st;
	if(_lookAheadBuf.size() <= count + _tokenPosStack.back()) {
		Lexer::Token dummy;
		st = lookAhead(count, dummy);
		if(!st) return st;
	}
	_tokenPosStack.back() += count;

	return roStatus::ok;
}

roStatus TokenStream::currentToken(Lexer::Token& token)
{
	return lookAhead(0, token);
}

roStatus TokenStream::lookAhead(roSize offset, Lexer::Token& token)
{
	while(_lookAheadBuf.size() <= _tokenPosStack.back() + offset) {
		Lexer::Token t;
		roStatus st = _lexer.nextToken(t);

		if(!st) {
			_tokenPosStack.back() += _tokenPosStack.back() < _lookAheadBuf.size() ? 1 : 0;
			return st;
		}

		bool skip = false;
		for(roSize i=0; i<tokenToIgnore.size(); ++i) {
			if(t.token == tokenToIgnore[i]) {
				skip = true;
				break;
			}
		}

		if(skip)
			continue;

		st = _lookAheadBuf.pushBack(t);
		if(!st) return st;
	}

	token = _lookAheadBuf[_tokenPosStack.back() + offset];
	return roStatus::ok;
}

roStatus TokenStream::endParse()
{
	return _lexer.endParse();
}

bool TokenStream::consumeToken(const RangedString& token)
{
	Token t;
	roSize consumed = 0;

	while(true) {
		if(!currentToken(t))
			return false;

		if(t.token == token) {
			consumeToken(1);
			++consumed;
		}
		else
			break;
	}

	return consumed > 0;
}

void TokenStream::pushState()
{
	roSize pos = _tokenPosStack.back();
	_tokenPosStack.pushBack(pos);
}

void TokenStream::restoreState()
{
	_tokenPosStack.popBack();
}

void TokenStream::discardState()
{
	roSize pos = _tokenPosStack.back();
	_tokenPosStack.popBack();
	_tokenPosStack.back() = pos;
}

TokenStream::StateScoped::StateScoped(TokenStream& tks)
	: tks(tks), consumed(false)
{
	tks.pushState();
}

TokenStream::StateScoped::~StateScoped()
{
	if(!consumed)
		tks.restoreState();
}

void TokenStream::StateScoped::consume()
{
	tks.discardState();
	consumed = true;
}

}	// namespace ro
