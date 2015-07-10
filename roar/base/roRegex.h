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
// Options:
// i	case-insensitive
// b	only match at beginning of string
struct Regex : private NonCopyable
{
	struct Graph;
	struct Compiled { Compiled(); ~Compiled(); String regexStr; Graph* graph; };

	Regex();

	// Matching with custom function. Use $0, $1... to refer to the function
	typedef bool (*CustomFunc)(RangedString& inout, void* userData);
	struct CustomMatcher {
		CustomFunc func;
		void* userData;
	};

	static roStatus compile(const roUtf8* reg, Compiled& compiled, roUint8 logLevel=0);
	static roStatus compile(const roUtf8* reg, Compiled& compiled, const IArray<CustomMatcher>& customMatcher, roUint8 logLevel=0);

	bool match(const roUtf8* str, const roUtf8* reg, const char* options=NULL);
	bool match(RangedString str, RangedString reg, const char* options=NULL);
	bool match(RangedString str, const Compiled& reg, const char* options=NULL);

	bool match(RangedString str, RangedString reg, const IArray<CustomMatcher>& customMatcher, const char* options=NULL);
	bool match(RangedString str, const Compiled& reg, const IArray<CustomMatcher>& customMatcher, const char* options=NULL);

	bool isMatch(roSize index) const;

	template<typename T>
	Status getValue(roSize index, T& value);
	Status getValue(roSize index, RangedString& str);

	template<typename T>
	T getValueWithDefault(roSize index, T defaultValue);

// Attributes
	roUint8	logLevel;	// 0 for no logging, larger for more
	TinyArray<RangedString, 16> result;

// Private
	bool _match(Graph& graph, const char* options);
	RangedString _srcString;	// For remembering the source string of the last performed match
};	// Regex

template<typename T>
Status Regex::getValue(roSize index, T& value)
{
	if(!result.isInRange(index))
		return Status::index_out_of_range;

	return roStrTo(result[index].begin, result[index].size(), value);
}

template<typename T>
T Regex::getValueWithDefault(roSize index, T defaultValue)
{
	T value;
	Status st = getValue(index, value);
	return st ? value : defaultValue;
}

}   // namespace ro

#endif	// __roRegex_h__
