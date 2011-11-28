#include "pch.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include <ctype.h>
#include <math.h>

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

char* roStrrStr(char* a, const char* b)
{
	roSize alen = roStrLen(a);
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

// Reference: http://www.bsdlover.cn/study/UnixTree/V7/usr/src/libc/gen/atof.c.html
double atof(const char* p, double onErr)
{
	if(!p) return onErr;

	char c;
	double fl, flexp, exp5;
	double big = 72057594037927936.;  /*2^56*/
	const int LOGHUGE = 39;	// 10^39 --> Largest power
	int nd;
	char eexp, exp, neg, negexp, bexp;

	neg = 1;
	while((c = *p++) == ' ')
		;
	if(c == '-')
		neg = -1;
	else if(c == '+')
		;
	else
		--p;

	exp = 0;
	fl = 0;
	nd = 0;
	while((c = *p++), isdigit(c)) {
		if(fl < big)
			fl = 10*fl + (c - '0');
		else
			++exp;
		++nd;
	}

	if(c == '.') {
		while((c = *p++), isdigit(c)) {
			if(fl < big) {
				fl = 10*fl + (c - '0');
				--exp;
			}
			++nd;
		}
	}

	negexp = 1;
	eexp = 0;
	if((c == 'E') || (c == 'e')) {
		if ((c = *p++) == '+')
			;
		else if(c == '-')
			negexp = -1;
		else
			--p;

		while((c = *p++), isdigit(c)) {
			eexp = 10 * eexp + (c - '0');
		}
		if (negexp<0)
			eexp = -eexp;
		exp += eexp;
	}

	negexp = 1;
	if (exp < 0) {
		negexp = -1;
		exp = -exp;
	}

	if((nd + exp * negexp) < -LOGHUGE) {
		fl = 0;
		exp = 0;
	}
	flexp = 1;
	exp5 = 5;
	bexp = exp;
	for(;;) {
		if(exp & 01)
			flexp *= exp5;
		exp >>= 1;
		if(exp == 0)
			break;
		exp5 *= exp5;
	}
	if(negexp < 0)
		fl /= flexp;
	else
		fl *= flexp;
	fl = ldexp(fl, negexp * bexp);
	if(neg < 0)
		fl = -fl;
	return fl;
}

bool roStrToBool(const char* str, bool defaultValue)
{
	if(!str)
		return defaultValue;
	else if(roStrCaseCmp(str, "true") == 0)
		return true;
	else if(roStrCaseCmp(str, "false") == 0)
		return false;

	return atof(str, defaultValue) > 0 ? true : false;
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
