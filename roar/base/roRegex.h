#ifndef __roRegex_h__
#define __roRegex_h__

#include "roArray.h"
#include "roString.h"
#include "roNonCopyable.h"

namespace ro {

// Reference: http://www.mikesdotnetting.com/Article/46/CSharp-Regular-Expressions-Cheat-Sheet
// \b	Matches a word boundary
// \B	Matches a non-word boundary
// \d	Matches a digit character. Equivalent to [0-9]
// \D	Matches a non-digit character. Equivalent to [^0-9]
// \s	Matches any white space including space, tab, form-feed, etc. Equivalent to "[ \f\n\r\t\v]"
// \S	Matches any nonwhite space character. Equivalent to "[^ \f\n\r\t\v]"
// \w	Matches any word character including underscore. Equivalent to "[A-Za-z0-9_]"
// \W	Matches any non-word character. Equivalent to "[^A-Za-z0-9_]"
struct Regex : private NonCopyable
{
	Regex();

	bool match(const roUtf8* s, const roUtf8* f, const char* options=NULL);

// Private
	struct SaveState;
	bool match_raw(const roUtf8*& s, const char*& f);
	bool match_charClass(const roUtf8*& s, const roUtf8*& f);
	bool match_repeat_or(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel, SaveState& ss);
	bool match_shortcut(const roUtf8*& s, const roUtf8*& f);
	bool match_shortcut_identifier(const roUtf8*& s, const roUtf8*& f);
	bool match_group(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel);
	bool match_beginOfString(const roUtf8*& s, const roUtf8*& f);
	bool match_endOfString(const roUtf8*& s, const roUtf8*& f);
	bool _match(const roUtf8*& s, const roUtf8*& f, roSize nestedLevel);

	bool (*charCmpFunc)(char c1, char c2);
	char* (*strChrFunc)(char* sz, char ch);
	const roUtf8* _sBegin, *_fBegin;
	const roUtf8* _matchBegin, *_matchEnd;
	Array<String> bracketResult;
};

}   // namespace ro

#endif	// __roRegex_h__
