#include "pch.h"
#include "roRegex.h"

namespace ro {

namespace {

struct Repeat {
	Repeat(bool success, bool keepTry) : success(success), keepTry(keepTry) {}
	bool success, keepTry;
};
Repeat repeatition(const roUtf8*& s, const roUtf8*& f, bool currentResult, roSize& successCount, roSize sIncOnSuccess)
{
	successCount += (currentResult ? 1 : 0);

	if(currentResult)
		s += sIncOnSuccess;

	if(*f == '?')
		return ++f, Repeat(true, false);

	if(*f == '*') {
		if(currentResult)
			return Repeat(true, true);
		else
			return ++f, Repeat(true, false);
	}

	if(*f == '+') {
		if(currentResult)
			return Repeat(true, true);
		else
			return ++f, Repeat(successCount > 0, false);
	}

	if(*f == '{') {
		roSize min = 0, max = 0;
		int ss = sscanf(f, "{%u,%u", &min, &max);
		if(ss != 1 && ss != 2)
			return Repeat(false, false);

		// Scan for '}'
		while(*(++f) != '}') {
			if(*f == '\0')
				return Repeat(false, false);
		}
		++f;

		if(min > max) max = min;

		bool keepTry = currentResult && (successCount < max);
		bool success = successCount >= min;
		return Repeat(success, keepTry);
	}

	return Repeat(currentResult, false);
}

static const char* symbols = "^*+?.|{}()[].";
static const char* escapes[] = { "nrtvf^*+?.|{}()[].", "\n\r\t\v\f^*+?.|{}()[]." };
static const char* shorcut = "sSbBdDwWnAZG";

static bool charCmp(char c1, char c2) { return c1 == c2; }
static bool charCaseCmp(char c1, char c2) { return roToLower(c1) == roToLower(c2); }

// Will eat '\' on successful escape
char doEscape(const roUtf8*& str)
{
	if(*str != '\\') return *str;
	const char* i = roStrChr(escapes[0], str[1]);
	if(!i) return '\0';
	return ++str, escapes[1][i - escapes[0]];
}

// Will eat '\' on successful scan
bool scanMeta(const roUtf8*& str, char c)
{
	if(*str != '\\') return *str == c;
	const char* i = roStrChr(escapes[0], str[1]);
	if(!i) return false;
	if(escapes[1][i - escapes[0]] == c)
		return ++str, true;
	return false;
}

}	// namespace

struct Regex::SaveState
{
	SaveState(const roUtf8*& s, const roUtf8*& f)
		: sRef(s), fRef(f), s(s), f(f), s1(s), f1(f)
	{}

	~SaveState()
	{
		revert();
		revert();
	}

	void commit()		{ commits(); commitf(); }
	void commits()		{ s1 = s; s = sRef; }
	void commitf()		{ f1 = f; f = fRef; }

	void revert()		{ reverts(); revertf(); }
	void reverts()		{ sRef = s1; s1 = s; }
	void revertf()		{ fRef = f1; f1 = f; }

	const roUtf8* s, *f, *s1, *f1;
	const roUtf8*& sRef, *&fRef;
};

Regex::Regex()
	: _sBegin(NULL) , _fBegin(NULL)
	, _matchBegin(NULL), _matchEnd(NULL)
{
}

bool Regex::match_raw(const roUtf8*& s, const char*& f)
{
	if(roStrChr(symbols, *f))
		return false;

	// Escape character
	bool isDot = (*f == '.');
	char escaped = doEscape(f);
	if(escaped == '\0' || *s == '\0')
		return false;

	SaveState ss(s, f);
	++f;
	Repeat rt(false, true);
	roSize successCount = 0;
	while(rt.keepTry) {
		rt = repeatition(s, f, (*charCmpFunc)(escaped, *s), successCount, 1);
	}

	if(rt.success)
		return ss.commit(), true;
	else
		return false;
}

bool Regex::match_charClass(const roUtf8*& s, const roUtf8*& f)
{
	if(*f != '[')
		return false;

	SaveState ss(s, f);
	++f;
	String charList;
	bool exclusion = false;

	if(*f == '^')
		exclusion = true, ++f;

	for(; true; ++f) {
		// Escape character
		char nonEscaped = *f;
		char escaped = doEscape(f);
		if(escaped == '\0') return false;

		// TODO: Check for invalid characters

		if(escaped == ']' && nonEscaped == ']' &&
			f[-1] != '[' &&	// []] is valid, treated as [\]]
			f[-1] != '^'	// [^]] is valid, treated as [^\]]
		) {
			++f;
			break;
		}

		if(escaped == '-' && nonEscaped == '-') {
			++f;
			if(charList.isEmpty())	// Handle the case [-abc], treated as [\-abc]
			{}
			else if(*f == ']')		// Handle the case [abc-], treated as [abc\-]
			{}
			else {
				escaped = doEscape(f);
				int dist = escaped - charList.back();
				if(dist < 0)
					return false;
				for(char c=charList.back()+1; c<escaped; ++c)
					charList.append(c);
			}
		}

		// Add to list
		charList.append(escaped);
	}

	// Match those in charList
	Repeat rt(false, true);
	roSize successCount = 0;
	if(!exclusion) {
		while(rt.keepTry) {
			rt = repeatition(s, f,
				*s != '\0' && (*strChrFunc)(charList.c_str(), *s),
				successCount, 1
			);
		}
	}
	else {
		while(rt.keepTry) {
			rt = repeatition(s, f,
				*s != '\0' && !(*strChrFunc)(charList.c_str(), *s),
				successCount, 1
			);
		}
	}

	if(rt.success)
		return ss.commit(), true;
	else
		return false;
}

bool Regex::match_repeat_or(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel, SaveState& ss_)
{
	SaveState ss(s, f);

	// If we immediately see '|', it means we already make a match
	if(*f == '|')
		goto Matched;

	const roUtf8* current = f;
	while(true) {
		// Otherwise, try to scan till next '|'
		for(; !scanMeta(f, '|'); ++f)
			if(*f == '\0')
				return false;

		if(_match(s, ++f, nestedLevel + 1))
			goto Matched;
	}

	return false;

Matched:
	// Eat all remaining regex characters till an ending ')'
	roSize openCount = 0;
	for(; !scanMeta(f, ')') || openCount > 0; ++f) {
		if(*f == '\0')
			break;

		if(scanMeta(f, '('))
			++openCount;
		else if(scanMeta(f, ')')) {
			if(openCount == 0)
				break;
			else
				--openCount;
		}
	}

	return ss.commit(), true;
}

bool Regex::match_shortcut_identifier(const roUtf8*& s, const roUtf8*& f)
{
	if(*f != 'i')
		return false;

	++f;
	if(!roIsAlpha(*s) && *s != '_')
		return false;

	for(; *(++s); )
	{
		if( !roIsAlpha(*s) && !roIsDigit(*s) && *s != '_' )
			break;
	}

	return true;
}

bool Regex::match_shortcut(const roUtf8*& s, const roUtf8*& f)
{
	if(*f != ':')
		return false;

	return match_shortcut_identifier(s, ++f);
}

bool Regex::match_group(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel)
{
	if(*f != '(')
		return false;

	SaveState ss(s, f);
	++f;

	const roUtf8* begin = s;
	const roUtf8* lastestSuccessBegin = NULL;

	Repeat rt(false, true);
	roSize successCount = 0;
	while(rt.keepTry) {
		begin = s;
		const roUtf8* fBak = f;
		bool success = _match(s, f, nestedLevel + 1) && scanMeta(f, ')');

		if(success) lastestSuccessBegin = begin;
		for(; !scanMeta(f, ')'); ++f)
			if(*f == '\0') return false;

		rt = repeatition(s, ++f, success, successCount, 0);
		if(rt.keepTry) f = fBak;
	}

	if(lastestSuccessBegin)
		bracketResult.pushBack(String(lastestSuccessBegin, s - lastestSuccessBegin));

	if(rt.success)
		return ss.commit(), true;
	else
		return false;
}

bool Regex::match_beginOfString(const roUtf8*& s, const roUtf8*& f)
{
	if(*f != '^') return false;
	if(s != _sBegin) return false;
	return ++f, true;
}

bool Regex::match_endOfString(const roUtf8*& s, const roUtf8*& f)
{
	if(*f != '$') return false;
	if(*s != '\0') return false;
	return ++f, true;
}

bool Regex::_match(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel)
{
	SaveState ss(s, f);
	bool ret = false;

	for(; true; ss.commit()) {
		bool b;
		ret |= b = match_raw(s, f);									if(b) continue;
		ret |= b = match_shortcut(s, f);							if(b) continue;
		ret |= b = match_group(s, f, nestedLevel);					if(b) continue;
		ret |= b = match_charClass(s, f);							if(b) continue;
		ret |= b = match_repeat_or(s, f, nestedLevel, ss);			if(b) continue;
		ret |= b = match_beginOfString(s, f);						if(b) continue;
		ret |= b = match_endOfString(s, f);							if(b) continue;
		break;
	}

	if(ret && nestedLevel == 0)
		_matchEnd = s;

	// Allow characters to be skipped at the beginning
	if(!ret && nestedLevel == 0) {
		f = _fBegin;
		while(*(++s)) {
			SaveState ss2(s, f);
			if(_match(s, f, nestedLevel + 1) && *f == '\0') {
				_matchBegin = ss2.s;
				_matchEnd = s;
				ss2.commit();
				ret = true;
				break;
			}
		}
	}

	return ss.commit(), ret;
}

bool Regex::match(const roUtf8* s, const roUtf8* f, const char* options)
{
	bracketResult.clear();
	charCmpFunc = charCmp;
	strChrFunc = roStrChr;

	if(options && roStrChr(options, 'i')) {
		charCmpFunc = charCaseCmp;
		strChrFunc = roStrChrCase;
	}

	const roUtf8* begin = s;
	_sBegin = s;
	_fBegin = f;
	_matchBegin = s;

	if(_match(s, f, 0) && *f == '\0')
		return bracketResult.insert(0, String(_matchBegin, _matchEnd-_matchBegin)), true;

	bracketResult.clear();
	return false;
}

}   // namespace ro
