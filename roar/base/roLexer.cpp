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

roStatus Lexer::nextToken(RangedString& token, RangedString& val, LineInfo& lineInfo)
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
	for(roSize i=0; i<longest.size(); ++i) {
		roSize len = longest[i].value.size();
		if(len < longestLen)
			continue;

		longestLen = len;
		token = longest[i].token;
		val = longest[i].value;
	}

	// Line, position counting
	lineInfo.l1 = lineInfo.l2;
	lineInfo.c1 = lineInfo.c2;
	lineInfo.p1 = lineInfo.p2;
	for(roSize i=0; i<val.size(); ++i) {
		++lineInfo.p2;
		if(val[i] == '\n') {
			++lineInfo.l2;
			lineInfo.c2 = 0;
		}
		else if(val[i] == '\r')
			lineInfo.c2 = 0;
		else
			++lineInfo.c2;
	}

	if(longest.isEmpty())
		return roStatus::end_of_data;

	src.begin = val.end;
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
		const char* found = roStrStr(inout.begin, inout.end, str.c_str());
		if(found)
			return inout.end = inout.begin + str.size(), true;
		return false;
	}
	String str;
};	// CustomRule

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
	virtual bool match(RangedString& inout) override
	{
		Regex regex;
		if(!regex.match(inout, this->regex, this->matcher, "b")) return false;
		inout.begin = regex.result[0].end;
		return true;
	}

	Regex::Compiled regex;
	TinyArray<Regex::CustomMatcher, 4> matcher;
};	// RegexRule

roStatus Lexer::registerRegexRule(const char* ruleName, const char* bnf, bool isFragment)
{
	roStatus st;

	if(_rules.find(ruleName))
		return roStatus::already_exist;

	RangedString str(bnf);
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
			if(!regex.match(result, "\\{([A-Za-z][A-za-z0-9]*)\\}"))
				continue;

			c = regex.result[0].end - 1;

			st = strFormat(regexStr, "${}", ruleNameCount);
			++ruleNameCount;

			IRule* r = _rules.find(regex.result[1].toString().c_str());
			if(!r) return roStatus::not_found;

			Regex::CustomMatcher regCustomMatcher = { _customMatch, r };
			st = rule->matcher.pushBack(regCustomMatcher);
			if(!st) return st;
		}
		else {
			st = regexStr.append(*c);
			if(!st) return st;
		}
	}

	st = Regex::compile(regexStr.c_str(), rule->regex);
	if(!st) return st;

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

}	// namespace ro
