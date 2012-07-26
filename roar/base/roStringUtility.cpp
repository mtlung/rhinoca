#include "pch.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>

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

template<class A, class B>
bool _castAssign(A a, B& b)
{
	if(!roCastAssert(a, b)) return false;
	b = B(a);
	return true;
}

template<class T>
static bool _parseNumber(const char* p, T& ret)
{
	// Skip white spaces
	static const char _whiteSpace[] = " \t\r\n";
	while(roStrChr(_whiteSpace, *p)) { ++p; }

	const bool neg = (*p) == '-';

	if(neg && ro::TypeOf<T>::isUnsigned())
		return false;

	if(neg) ++p;
	while(roStrChr(_whiteSpace, *p)) { ++p; }

	// Skip leading zeros
	bool hasLeadingZeros = false;
	while(*p == '0') { hasLeadingZeros = true; ++p; }

	static const T preOverMax = ro::TypeOf<T>::valueMax() / 10;
	static const char preOverMax2 = char(ro::TypeOf<T>::valueMax() % 10) + '0';
	static const T preUnderMin = -(roInt64)(ro::TypeOf<T>::valueMin() / 10);
	static const char preUnderMin2 = '0' - char(ro::TypeOf<T>::valueMin() % 10);

	T i = 0;

	// Ensure there is really some numbers in the string
	if(*p >= '1' && *p <= '9')
		i = *(p++) - '0';
	else {
		ret = i;
		return hasLeadingZeros;
	}

	if(ro::TypeOf<T>::isUnsigned() || !neg)
	{
		while(*p >= '0' && *p <= '9') {
			if(i >= preOverMax) {
				if(i != preOverMax || *p > preOverMax2)
					return false;
			}
			i = i * 10 + (*p - '0');
			++p;
		}
	}
	else
	{
		while(*p >= '0' && *p <= '9') {
			if(i >= preUnderMin) {
				if(i != preUnderMin || *p > preUnderMin2)
					return false;
			}
			i = i * 10 + (*p - '0');
			++p;
		}
	}

	ret = i;
	return true;
}

bool roStrTo(const char* str, bool& ret)
{
	if(roStrCaseCmp(str, "true") == 0) {
		ret = true;
		return true;
	}
	else if(roStrCaseCmp(str, "false") == 0) {
		ret = false;
		return true;
	}

	roUint8 tmp;
	if(!roStrTo(str, tmp))
		return false;

	ret = (tmp == 1);
	return true;
}

bool roStrTo(const char* str, float& ret) {
	return sscanf(str, "%f", &ret) == 1;
}

bool roStrTo(const char* str, double& ret) {
	return sscanf(str, "%lf", &ret) == 1;
}

bool roStrTo(const char* str, roInt8& ret) {
	int tmp; return roStrTo(str, tmp) && _castAssign(tmp, ret);
}

bool roStrTo(const char* str, roInt16& ret) {
	int tmp; return roStrTo(str, tmp) && _castAssign(tmp, ret);
}

bool roStrTo(const char* str, roInt32& ret) {
	return _parseNumber(str, ret);
}

bool roStrTo(const char* str, roInt64& ret) {
	return _parseNumber(str, ret);
}

bool roStrTo(const char* str, roUint8& ret) {
	unsigned tmp; return roStrTo(str, tmp) && _castAssign(tmp, ret);
}

bool roStrTo(const char* str, roUint16& ret) {
	unsigned tmp; return roStrTo(str, tmp) && _castAssign(tmp, ret);
}

bool roStrTo(const char* str, roUint32& ret) {
	return _parseNumber(str, ret);
}

bool roStrTo(const char* str, roUint64& ret) {
	return _parseNumber(str, ret);
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

// Some notes on printf:
// http://www.dgp.toronto.edu/~ajr/209/notes/printf.html
roSize roToString(char* str, roSize strBufSize, float val, const char* option)
{
	int ret = 0;
#if roCOMPILER_VC
	ret = _snprintf(str, strBufSize, "%g", val);
#else
#endif

	return roMaxOf2(ret, 0);
}

roSize roToString(char* str, roSize strBufSize, double val, const char* option)
{
	int ret = 0;
#if roCOMPILER_VC
	ret = _snprintf(str, strBufSize, "%lg", val);
#else
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
	roInt64 val = val_;

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
		utf8[0] = num_cast<roUtf8>((c >> 6) | 0xC0);
		utf8[1] = num_cast<roUtf8>((c & 0x3F) | 0x80);
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
