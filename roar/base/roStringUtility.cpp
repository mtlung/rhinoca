#include "pch.h"
#include "roStringUtility.h"
#include "roString.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>

// Some notes on printf:
// http://www.dgp.toronto.edu/~ajr/209/notes/printf.html

char* roStrStr(char* a, const char* b)
{
	char* sa = a;
	const char* sb = b;
	if(*b == 0) return NULL;
	for( ; *a; ++a) {
		sa = a;
		sb = b;
		for(;;) {
			if(*sb == 0) return a;	// Found
			if(*sb != *sa) break;
			++sa; ++sb;
		}
	}
	return NULL;
}

char* roStrStr(char* a, const char* aEnd, const char* b)
{
	return roStrnStr(a, aEnd - a, b);
}

char* roStrnStr(char* a, roSize aLen, const char* b)
{
	char* sa = a;
	const char* sb = b;
	if(*b == 0) return NULL;
	for(roSize i=0; i < aLen; ++i, ++a) {
		sa = a;
		sb = b;
		for(;;) {
			if(*sb == 0) return a;	// Found
			if(*sb != *sa) break;
			++sa; ++sb;
		}
	}
	return NULL;
}

char* roStrrStr(char* a, const char* b)
{
	return roStrrnStr(a, roStrLen(a), b);
}

char* roStrrnStr(char* a, roSize aLen, const char* b)
{
	roSize alen = aLen;
	roSize blen = roStrLen(b);

	if(blen > alen) return NULL;

	for(char* p = a + alen - blen; p >= a; --p) {
		if(roStrnCmp(p, b, blen) == 0)
			return p;
	}

	return NULL;
}

char* roStrStrCase(char* a, const char* b)
{
	char* sa = a;
	const char* sb = b;
	if(*b == 0) return NULL;
	for( ; *a; ++a) {
		sa = a;
		sb = b;
		for(;;) {
			if(*sb == 0) return a;	// Found
			if(roToLower(*sb) != roToLower(*sa)) break;
			++sa; ++sb;
		}
	}
	return NULL;
}

char* roStrStrCase(char* a, const char* aEnd, const char* b)
{
	return roStrnStrCase(a, aEnd - a, b);
}

char* roStrnStrCase(char* a, roSize aLen, const char* b)
{
	char* sa = a;
	const char* sb = b;
	if(*b == 0) return NULL;
	for(roSize i=0; i < aLen; ++i, ++a) {
		sa = a;
		sb = b;
		for(;;) {
			if(*sb == 0) return a;	// Found
			if(roToLower(*sb) != roToLower(*sa)) break;
			++sa; ++sb;
		}
	}
	return NULL;
}

void roToLower(char* str)
{
	while(*str != '\0') {
		*str = roToLower(*str);
		++str;
	}
}

void roToUpper(char* str)
{
	while(*str != '\0') {
		*str = (char)roToUpper(*str);
		++str;
	}
}

void roStrReverse(char *str, roSize len)
{
	if(len <= 1) return;
	for(roSize i=0; i<=(len - 1)/2; ++i)
		roSwap(str[i], str[len-1-i]);
}


// ----------------------------------------------------------------------

template<class T>
static roStatus _parseNumber(const char* p, const char* end, T& ret, const char** newp=NULL)
{
	if(!p)
		return roStatus::pointer_is_null;

	if(p >= end && end)
		return roStatus::string_parsing_error;

	// Skip white spaces
	static const char _whiteSpace[] = " \t\r\n";
	while(roStrChr(_whiteSpace, *p)) {
		if(++p >= end && end)
			return roStatus::string_parsing_error;
	}

	const bool neg = (*p) == '-';

	if(neg && ro::TypeOf<T>::isUnsigned())
		return roStatus::string_to_number_overflow;

	if(neg) ++p;
	if(p >= end && end)
		return roStatus::string_parsing_error;

	while(roStrChr(_whiteSpace, *p)) {
		if(++p >= end && end)
			return roStatus::string_parsing_error;
	}

	T i = 0;

	// Skip leading zeros
	bool hasLeadingZeros = false;
	while(*p == '0') {
		hasLeadingZeros = true;
		if(++p >= end && end)
			goto Done;
	}

	static const T preOverMax = ro::TypeOf<T>::valueMax() / 10;
	static const char preOverMax2 = char(ro::TypeOf<T>::valueMax() % 10) + '0';
	static const T preUnderMin = -(roInt64)(ro::TypeOf<T>::valueMin() / 10);
	static const char preUnderMin2 = '0' - char(ro::TypeOf<T>::valueMin() % 10);

	T preCheck;
	char preCheck2;
	if(ro::TypeOf<T>::isUnsigned() || !neg) {
		preCheck = preOverMax;
		preCheck2 = preOverMax2;
	}
	else {
		preCheck = preUnderMin;
		preCheck2 = preUnderMin2;
	}

	while((p < end || !end) && roIsDigit(*p)) {
		if(i >= preCheck && (i != preCheck || *p > preCheck2))
			return roStatus::string_to_number_overflow;
		i = i * 10 + (*p - '0');
		++p;
	}

	if(neg)
		i *= (T)-1;

Done:
	if(i == 0 && !hasLeadingZeros)
		return roStatus::string_parsing_error;

	if(newp) *newp = p;
	ret = i;
	return roStatus::ok;
}

roStatus roStrTo(const char* str, bool& ret)
{
	if(roStrCaseCmp(str, "true") == 0) {
		ret = true;
		return roStatus::ok;
	}
	else if(roStrCaseCmp(str, "false") == 0) {
		ret = false;
		return roStatus::ok;
	}

	roUint64 tmp;
	roStatus st = roStrTo(str, tmp);
	if(!st) return st;

	ret = (tmp == 1);
	return roStatus::ok;
}

roStatus roStrTo(const char* str, float& ret) {
	return sscanf(str, "%f", &ret) == 1 ? roStatus::ok : roStatus::string_parsing_error;
}

roStatus roStrTo(const char* str, double& ret) {
	return sscanf(str, "%lf", &ret) == 1 ? roStatus::ok : roStatus::string_parsing_error;
}

roStatus roStrTo(const char* str, roInt8& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roInt16& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roInt32& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roInt64& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roUint8& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roUint16& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roUint32& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, roUint64& ret) {
	return _parseNumber(str, NULL, ret);
}

roStatus roStrTo(const char* str, ro::String& ret) {
	return ret.assign(str);
}

roStatus roStrTo(const char* str, roSize len, bool& ret)
{
	if(len == 4 && roStrCaseCmp(str, "true") == 0) {
		ret = true;
		return roStatus::ok;
	}
	else if(len == 5 && roStrCaseCmp(str, "false") == 0) {
		ret = false;
		return roStatus::ok;
	}

	roUint64 tmp;
	roStatus st = roStrTo(str, len, tmp);
	if(!st) return st;

	ret = (tmp == 1);
	return roStatus::ok;
}

roStatus roStrTo(const char* str, roSize len, float& ret) {
	return roStatus::not_implemented;
//	return sscanf(str, "%f", &ret) == 1 ? roStatus::ok : roStatus::string_parsing_error;
}

roStatus roStrTo(const char* str, roSize len, double& ret) {
	return roStatus::not_implemented;
//	return sscanf(str, "%lf", &ret) == 1 ? roStatus::ok : roStatus::string_parsing_error;
}

roStatus roStrTo(const char* str, roSize len, roInt8& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roInt16& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roInt32& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roInt64& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roUint8& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roUint16& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roUint32& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, roUint64& ret) {
	return _parseNumber(str, str + len, ret);
}

roStatus roStrTo(const char* str, roSize len, ro::String& ret) {
	return ret.assign(str, len);
}

roStatus roStrTo(const char* str, const char*& newPos, roInt8& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roInt16& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roInt32& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roInt64& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roUint8& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roUint16& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roUint32& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

roStatus roStrTo(const char* str, const char*& newPos, roUint64& ret) {
	return _parseNumber(str, NULL, ret, &newPos);
}

bool roStrToBool(const char* str, bool defaultValue) {
	bool ret; return roStrTo(str, ret) ? ret : defaultValue;
}

float roStrToFloat(const char* str, float defaultValue) {
	float ret; return roStrTo(str, ret) ? ret : defaultValue;
}

double roStrToDouble(const char* str, double defaultValue) {
	double ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roInt8 roStrToInt8(const char* str, roInt8 defaultValue) {
	roInt8 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roInt16 roStrToInt16(const char* str, roInt16 defaultValue) {
	roInt16 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roInt32 roStrToInt32(const char* str, roInt32 defaultValue) {
	roInt32 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roInt64 roStrToInt64(const char* str, roInt64 defaultValue) {
	roInt64 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roUint8 roStrToUint8(const char* str, roUint8 defaultValue) {
	roUint8 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roUint16 roStrToUint16(const char* str, roUint16 defaultValue) {
	roUint16 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roUint32 roStrToUint32(const char* str, roUint32 defaultValue) {
	roUint32 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

roUint64 roStrToUint64(const char* str, roUint64 defaultValue) {
	roUint64 ret; return roStrTo(str, ret) ? ret : defaultValue;
}

bool roStrToBool(const char* str, roSize len, bool defaultValue) {
	bool ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

float roStrToFloat(const char* str, roSize len, float defaultValue) {
	float ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

double roStrToDouble(const char* str, roSize len, double defaultValue) {
	double ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roInt8 roStrToInt8(const char* str, roSize len, roInt8 defaultValue) {
	roInt8 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roInt16 roStrToInt16(const char* str, roSize len, roInt16 defaultValue) {
	roInt16 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roInt32 roStrToInt32(const char* str, roSize len, roInt32 defaultValue) {
	roInt32 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roInt64 roStrToInt64(const char* str, roSize len, roInt64 defaultValue) {
	roInt64 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roUint8 roStrToUint8(const char* str, roSize len, roUint8 defaultValue) {
	roUint8 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roUint16 roStrToUint16(const char* str, roSize len, roUint16 defaultValue) {
	roUint16 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roUint32 roStrToUint32(const char* str, roSize len, roUint32 defaultValue) {
	roUint32 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

roUint64 roStrToUint64(const char* str, roSize len, roUint64 defaultValue) {
	roUint64 ret; return roStrTo(str, len, ret) ? ret : defaultValue;
}

static char _hexCharValue(char h)
{
	if(roIsDigit(h))
		return h - '0';

	char lower = roToLower(h);
	if((lower - 'a' + 0u) <= ('f' - 'a' + 0u))
		return lower - 'a' + 10;

	return -1;
}

template<class T>
static roStatus _parseHex(const char* p, const char* end, T& ret, const char** newp=NULL)
{
	if(!p)
		return roStatus::pointer_is_null;

	if(p >= end && end)
		return roStatus::string_parsing_error;

	// Skip white spaces
	static const char _whiteSpace[] = " \t\r\n";
	while(roStrChr(_whiteSpace, *p)) {
		if(++p >= end && end)
			return roStatus::string_parsing_error;
	}

	while(roStrChr(_whiteSpace, *p)) {
		if(++p >= end && end)
			return roStatus::string_parsing_error;
	}

	T i = 0;

	// Skip leading zeros
	bool hasLeadingZeros = false;
	while(*p == '0') {
		hasLeadingZeros = true;
		if(++p >= end && end)
			goto Done;
	}

	// TODO: Deal with 0x

	static const T preOverMax = ro::TypeOf<T>::valueMax() / 16;
	static const char preOverMax2 = char(ro::TypeOf<T>::valueMax() % 16) + '0';

	while((p < end || !end)) {
		char val = _hexCharValue(*p);
		if(val < 0)
			break;

		if(i >= preOverMax && (i != preOverMax || val > preOverMax2))
			return roStatus::string_to_number_overflow;
		i = i * 16 + val;
		++p;
	}

Done:
	if(i == 0 && !hasLeadingZeros)
		return roStatus::string_parsing_error;

	if(newp) *newp = p;
	ret = i;
	return roStatus::ok;
}

roStatus roHexStrTo(const char* str, roUint8& ret) {
	return _parseHex(str, NULL, ret);
}

roStatus roHexStrTo(const char* str, roUint16& ret) {
	return _parseHex(str, NULL, ret);
}

roStatus roHexStrTo(const char* str, roUint32& ret) {
	return _parseHex(str, NULL, ret);
}

roStatus roHexStrTo(const char* str, roUint64& ret) {
	return _parseHex(str, NULL, ret);
}

roStatus roHexStrTo(const char* str, roSize len, roUint8& ret) {
	return _parseHex(str, str + len, ret);
}

roStatus roHexStrTo(const char* str, roSize len, roUint16& ret) {
	return _parseHex(str, str + len, ret);
}

roStatus roHexStrTo(const char* str, roSize len, roUint32& ret) {
	return _parseHex(str, str + len, ret);
}

roStatus roHexStrTo(const char* str, roSize len, roUint64& ret) {
	return _parseHex(str, str + len, ret);
}

roStatus roHexStrTo(const char* str, const char*& newPos, roUint8& ret) {
	return _parseHex(str, NULL, ret, &newPos);
}

roStatus roHexStrTo(const char* str, const char*& newPos, roUint16& ret) {
	return _parseHex(str, NULL, ret, &newPos);
}

roStatus roHexStrTo(const char* str, const char*& newPos, roUint32& ret) {
	return _parseHex(str, NULL, ret, &newPos);
}

roStatus roHexStrTo(const char* str, const char*& newPos, roUint64& ret) {
	return _parseHex(str, NULL, ret, &newPos);
}

roUint8 roHexStrToUint8(const char* str, roUint8 defaultValue) {
	roUint8 ret; return roHexStrTo(str, ret) ? ret : defaultValue;
}

roUint16 roHexStrToUint16(const char* str, roUint16 defaultValue) {
	roUint16 ret; return roHexStrTo(str, ret) ? ret : defaultValue;
}

roUint32 roHexStrToUin32(const char* str, roUint32 defaultValue) {
	roUint32 ret; return roHexStrTo(str, ret) ? ret : defaultValue;
}

roUint64 roHexStrToUint64(const char* str, roUint64 defaultValue) {
	roUint64 ret; return roHexStrTo(str, ret) ? ret : defaultValue;
}

roUint8 roHexStrToUint8(const char* str, roSize len, roUint8 defaultValue) {
	roUint8 ret; return roHexStrTo(str, len, ret) ? ret : defaultValue;
}

roUint16 roHexStrToUint16(const char* str, roSize len, roUint16 defaultValue) {
	roUint16 ret; return roHexStrTo(str, len, ret) ? ret : defaultValue;
}

roUint32 roHexStrToUint32(const char* str, roSize len, roUint32 defaultValue) {
	roUint32 ret; return roHexStrTo(str, len, ret) ? ret : defaultValue;
}

roUint64 roHexStrToUint64(const char* str, roSize len, roUint64 defaultValue) {
	roUint64 ret; return roHexStrTo(str, len, ret) ? ret : defaultValue;
}

// ----------------------------------------------------------------------

roSize roToString(char* str, roSize strBufSize, bool val, const char* option)
{
	if(!str)
		return 1;

	if(strBufSize < 2)
		return 0;
	str[0] = val ? '1' : '0';
	str[1] = '\0';
	return 1;
}

roSize roToString(char* str, roSize strBufSize, float val, const char* option)
{
	int ret = 0;
#if roCOMPILER_VC
	ret = _snprintf(str, strBufSize, "%.3g", val);
#else
	ret = snprintf(str, strBufSize, "%.3g", val);
#endif

	return roMaxOf2(ret, 0);
}

roSize roToString(char* str, roSize strBufSize, roInt8 val, const char* option)
{
	return roToString(str, strBufSize, roInt64(val));
}

roSize roToString(char* str, roSize strBufSize, roInt16 val, const char* option)
{
	return roToString(str, strBufSize, roInt64(val));
}

roSize roToString(char* str, roSize strBufSize, roInt32 val, const char* option)
{
	return roToString(str, strBufSize, roInt64(val));
}

roSize roToString(char* str, roSize strBufSize, roInt64 val_, const char* option)
{
	roSize written = 0;
	roInt64 val = val_;

	if(!str) {
		if(val < 0) ++written;
		do {
			++written;
			val /= 10;
		} while(val);
		return written;
	}
	else {
		roSize max = strBufSize;
		if(val < 0) {
			if(written >= max)
				return 0;
			str[written++] = '-';
			val = -val;
		}

		roSize strIdx = written;

		do {
			if(written >= max)
				return 0;

			str[written++] = '0' + (val % 10);
			val /= 10;
		} while(val);
		roStrReverse(str + strIdx, *str == '-' ? written - 1 : written);

		// Write the terminator only when there is space left, as if in printf
		if(written < strBufSize)
			str[written] = '\0';

		return written;
	}
}

roSize roToString(char* str, roSize strBufSize, roUint8 val, const char* option)
{
	return roToString(str, strBufSize, roUint64(val));
}

roSize roToString(char* str, roSize strBufSize, roUint16 val, const char* option)
{
	return roToString(str, strBufSize, roUint64(val));
}

roSize roToString(char* str, roSize strBufSize, roUint32 val, const char* option)
{
	return roToString(str, strBufSize, roUint64(val));
}

roSize roToString(char* str, roSize strBufSize, roUint64 val_, const char* option)
{
	roSize written = 0;
	roUint64 val = val_;

	if(!str) {
		do {
			++written;
			val /= 10;
		} while(val);
		return written;
	}
	else {
		roSize max = strBufSize;
		do {
			if(written >= max)
				return 0;

			str[written++] = '0' + (val % 10);
			val /= 10;
		} while(val);
		roStrReverse(str, written);

		// Write the terminator only when there is space left, as if in printf
		if(written < strBufSize)
			str[written] = '\0';

		return written;
	}
}

static const char _hexCharTable[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

roSize roToString(char* str, roSize strBufSize, const void* ptrVal, const char* option)
{
	roSize written = 0;
	roUint64 val = roUint64(ptrVal);

	if(!str) {
		do {
			++written;
			val /= 16;
		} while(val);
		return written;
	}
	else {
		roSize max = strBufSize;
		do {
			if(written >= max)
				return 0;

			str[written++] = _hexCharTable[val % 16];
			val /= 16;
		} while(val);
		roStrReverse(str, written);

		// Write the terminator only when there is space left, as if in printf
		if(written < strBufSize)
			str[written] = '\0';

		return written;
	}
}


// ----------------------------------------------------------------------

int roUtf8ToUtf16Char(roUtf16& out, const roUtf8* utf8, roSize len)
{
	if(len < 1) return 0;
	if((utf8[0] & 0x80) == 0) {
		out = (roUtf16)utf8[0];
		return 1;
	}
	if((utf8[0] & 0xE0) == 0xC0) {
		if(len < 2) return 0;
		out = (roUtf16)(utf8[0] & 0x1F) << 6
			| (roUtf16)(utf8[1] & 0x3F);
		return 2;
	}
	if((utf8[0] & 0xF0) == 0xE0) {
		if(len < 3) return 0;
		out = (roUtf16)(utf8[0] & 0x0F) << 12
			| (roUtf16)(utf8[1] & 0x3F) << 6
			| (roUtf16)(utf8[2] & 0x3F);
		return 3;
	}
	return -1;
}

int roUtf16ToUtf8(roUtf8* utf8, roSize utf8Len, roUtf16 c)
{
	if(c <= 0x7F) {
		if(utf8Len < 1) return -1;
		utf8[0] = (roUtf8)c;
		return 1;
	}
	if(c <= 0x7FF) {
		if(utf8Len < 2) return -2;
		utf8[0] = num_cast<roUint8>((c >> 6) | 0xC0);
		utf8[1] = num_cast<roUint8>((c & 0x3F) | 0x80);
		return 2;
	}
	if(utf8Len < 3) return -3;
	utf8[0] = (c >> 12) | 0xE0;
	utf8[1] = ((c >> 6) & 0x3F) | 0x80;
	utf8[2] = (c & 0x3F) | 0x80;
	return 3;
}

roStatus roUtf8CountInUtf16(roSize& outLen, const roUtf16* utf16)
{
	if(!utf16) return roStatus::invalid_parameter;
	outLen = 0;
	for(; *utf16; ++utf16) {
		if(*utf16 <= 0x7F ) { outLen += 1; continue; }
		if(*utf16 <= 0x7FF) { outLen += 2; continue; }
		outLen += 3;
	}
	return roStatus::ok;
}

roStatus roUtf8CountInUtf16(roSize& outLen, const roUtf16* utf16, roSize utf16len)
{
	if(!utf16) return roStatus::invalid_parameter;
	outLen = 0;
	for(roSize i=0; i<utf16len && *utf16; ++i, ++utf16) {
		if(*utf16 <= 0x7F ) { outLen += 1; continue; }
		if(*utf16 <= 0x7FF) { outLen += 2; continue; }
		outLen += 3;
	}
	return roStatus::ok;
}

roStatus roUtf16CountInUtf8(roSize& outLen, const roUtf8* utf8)
{
	if(!utf8) return roStatus::invalid_parameter;
	outLen = 0;
	for(; *utf8; ++utf8, ++outLen) {
		if((*utf8 & 0x80) == 0) {
			continue;
		}
		if((*utf8 & 0xE0) == 0xC0) {
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			continue;
		}
		if((*utf8 & 0xF0) == 0xE0) {
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			continue;
		}
	}
	return roStatus::ok;
}

roStatus roUtf16CountInUtf8(roSize& outLen, const roUtf8* utf8, roSize utf8Len)
{
	if(!utf8) return roStatus::invalid_parameter;
	outLen = 0;
	for(roSize i=0; i<utf8Len && *utf8; ++i, ++utf8, ++outLen) {
		if((*utf8 & 0x80) == 0) {
			continue;
		}
		if((*utf8 & 0xE0) == 0xC0) {
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			continue;
		}
		if((*utf8 & 0xF0) == 0xE0) {
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			++utf8; if(*utf8 == 0) return roStatus::string_encoding_error;
			continue;
		}
	}
	return roStatus::ok;
}

roStatus roUtf8ToUtf16(roUtf16* dst, roSize& dstLen, const roUtf8* src)
{
	if(!src) return roStatus::invalid_parameter;
	if(!dst) return roUtf16CountInUtf8(dstLen, src);

	for(roSize srcLen = roStrLen(src), dLen=dstLen; srcLen && dLen;) {
		int utf8Consumed = roUtf8ToUtf16Char(*dst, src, srcLen);
		if(utf8Consumed == 0) return roStatus::string_encoding_error;
		roAssert(srcLen >= roSize(utf8Consumed));
		++dst;
		--dLen;
		src += utf8Consumed;
		srcLen -= utf8Consumed;
	}

	return roStatus::ok;
}

roStatus roUtf8ToUtf16(roUtf16* dst, roSize& dstLen, const roUtf8* src, roSize maxSrcLen)
{
	if(!src) return roStatus::invalid_parameter;
	if(!dst) return roUtf16CountInUtf8(dstLen, src, maxSrcLen);

	for(roSize dLen = dstLen; maxSrcLen && dLen;) {
		int utf8Consumed = roUtf8ToUtf16Char(*dst, src, maxSrcLen);
		if(utf8Consumed == 0) return roStatus::string_encoding_error;
		roAssert(maxSrcLen >= roSize(utf8Consumed));
		++dst;
		--dLen;
		src += utf8Consumed;
		maxSrcLen -= utf8Consumed;
	}

	return roStatus::ok;
}

roStatus roUtf16ToUtf8(roUtf8* dst, roSize& dstLen, const roUtf16* src)
{
	if(!src) return roStatus::invalid_parameter;
	if(!dst) return roUtf8CountInUtf16(dstLen, src);

	for(roSize srcLen = roStrLen(src), dLen = dstLen; srcLen && dstLen;) {
		int utf8Written = roUtf16ToUtf8(dst, dLen, *src);
		if(utf8Written < 1) return roStatus::string_encoding_error;
		++src;
		--srcLen;
		dst += utf8Written;
		dLen -= utf8Written;
	}

	return roStatus::ok;
}

roStatus roUtf16ToUtf8(roUtf8* dst, roSize& dstLen, const roUtf16* src, roSize maxSrcLen)
{
	if(!src) return roStatus::invalid_parameter;
	if(!dst) return roUtf8CountInUtf16(dstLen, src, maxSrcLen);

	for(roSize dLen = dstLen; maxSrcLen && dLen;) {
		int utf8Written = roUtf16ToUtf8(dst, dstLen, *src);
		if(utf8Written < 1) return roStatus::string_encoding_error;
		++src;
		--maxSrcLen;
		dst += utf8Written;
		dLen -= utf8Written;
	}

	return roStatus::ok;
}

// Reference: http://en.wikipedia.org/wiki/Utf8
static const roUint8 _utf8Limits[] = {
	0xC0,	// Start of a 2-byte sequence
	0xE0,	// Start of a 3-byte sequence
	0xF0,	// Start of a 4-byte sequence
	0xF8,	// Start of a 5-byte sequence
	0xFC,	// Start of a 6-byte sequence
	0xFE	// Invalid: not defined by original UTF-8 specification
};
