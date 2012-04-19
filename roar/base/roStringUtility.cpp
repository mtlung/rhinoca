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

// Reference: http://en.wikipedia.org/wiki/Utf8
static const roUint8 _utf8Limits[] = {
	0xC0,	// Start of a 2-byte sequence
	0xE0,	// Start of a 3-byte sequence
	0xF0,	// Start of a 4-byte sequence
	0xF8,	// Start of a 5-byte sequence
	0xFC,	// Start of a 6-byte sequence
	0xFE	// Invalid: not defined by original UTF-8 specification
};

bool roUtf8ToUtf16(roUint16* dest, roSize& destLen, const char* src, roSize maxSrcLen)
{
	if(!src)
		return false;

	roSize destPos = 0, srcPos = 0;

	while(true)
	{
		if(srcPos == maxSrcLen || src[srcPos] == '\0') {
			if(dest && destLen != destPos) {
				roAssert(false && "The provided destLen should equals to what we calculated here");
				return false;
			}

			destLen = destPos;
			return true;
		}

		roUint8 c = src[srcPos++];

		if(c < 0x80) {	// 0-127, US-ASCII (single byte)
			if(dest)
				dest[destPos] = c;
			++destPos;
			continue;
		}

		if(c < 0xC0)	// The first octet for each code point should within 0-191
			break;

		roSize numAdds = 1;
		for(; numAdds < 5; ++numAdds)
			if(c < _utf8Limits[numAdds])
				break;
		roUint32 value = c - _utf8Limits[numAdds - 1];

		do {
			if(srcPos == maxSrcLen || src[srcPos] == '\0')
				break;
			roUint8 c2 = src[srcPos++];
			if(c2 < 0x80 || c2 >= 0xC0)
				break;
			value <<= 6;
			value |= (c2 - 0x80);
		} while(--numAdds != 0);

		if(value < 0x10000) {
			if(dest)
				dest[destPos] = num_cast<roUint16>(value);
			++destPos;
		}
		else {
			value -= 0x10000;
			if(value >= 0x100000)
				break;
			if(dest) {
				dest[destPos + 0] = num_cast<roUint16>(0xD800 + (value >> 10));
				dest[destPos + 1] = num_cast<roUint16>(0xDC00 + (value & 0x3FF));
			}
			destPos += 2;
		}
	}

	destLen = destPos;
	return false;
}

bool roUtf8ToUtf32(roUint32* dest, roSize& destLen, const char* src, roSize maxSrcLen)
{
	if(!src)
		return false;

	roSize destPos = 0, srcPos = 0;

	while(true)
	{
		if(srcPos == maxSrcLen || src[srcPos] == '\0') {
			if(dest && destLen != destPos) {
				roAssert(false && "The provided destLen should equals to what we calculated here");
				return false;
			}

			destLen = destPos;
			return true;
		}

		roUint8 c = src[srcPos++];

		if(c < 0x80) {	// 0-127, US-ASCII (single byte)
			if(dest)
				dest[destPos] = num_cast<roUint32>(c);
			++destPos;
			continue;
		}

		if(c < 0xC0)	// The first octet for each code point should within 0-191
			break;

		roSize numAdds = 1;
		for(; numAdds < 5; ++numAdds)
			if(c < _utf8Limits[numAdds])
				break;
		roUint32 value = c - _utf8Limits[numAdds - 1];

		do {
			if(srcPos == maxSrcLen || src[srcPos] == '\0')
				break;
			roUint8 c2 = src[srcPos++];
			if(c2 < 0x80 || c2 >= 0xC0)
				break;
			value <<= 6;
			value |= (c2 - 0x80);
		} while(--numAdds != 0);

		if(value < 0x10000) {
			if(dest)
				dest[destPos] = value;
			++destPos;
		}
		else {
			value -= 0x10000;
			if(value >= 0x100000)
				break;
			if(dest)
				dest[destPos] = value;
			++destPos;
		}
	}

	destLen = destPos;
	return false;
}

bool roUtf16ToUtf8(char* dest, roSize& destLen, const roUint16* src, roSize maxSrcLen)
{
	if(!src)
		return false;

	roSize destPos = 0, srcPos = 0;

	while(true)
	{
		if(srcPos == maxSrcLen || src[srcPos] == L'\0') {
			if(dest && destLen != destPos) {
				roAssert(false && "The provided destLen should equals to what we calculated here");
				return false;
			}
			destLen = destPos;
			return true;
		}

		roUint32 value = src[srcPos++];

		if(value < 0x80) {	// 0-127, US-ASCII (single byte)
			if(dest)
				dest[destPos] = num_cast<char>(value);
			++destPos;
			continue;
		}

		if(value >= 0xD800 && value < 0xE000) {
			if(value >= 0xDC00 || srcPos == maxSrcLen)
				break;
			roUint32 c2 = src[srcPos++];
			if(c2 < 0xDC00 || c2 >= 0xE000)
				break;
			value = ((value - 0xD800) << 10) | (c2 - 0xDC00);
		}

		roSize numAdds = 1;
		for(; numAdds < 5; ++numAdds)
			if(value < (1u << (numAdds * 5 + 6)))
				break;

		if(dest)
			dest[destPos] = num_cast<char>(_utf8Limits[numAdds - 1] + (value >> (6 * numAdds)));
		++destPos;

		do {
			--numAdds;
			if(dest)
				dest[destPos] = num_cast<char>(0x80 + ((value >> (6 * numAdds)) & 0x3F));
			++destPos;
		} while(numAdds != 0);
	}

	destLen = destPos;
	return false;
}
