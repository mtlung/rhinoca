#include "pch.h"
#include "roStringUtility.h"
#include "roUtility.h"

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
