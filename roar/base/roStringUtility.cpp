#include "pch.h"
#include "roStringUtility.h"
#include "roUtility.h"

void roStrToLower(char* str)
{
	while(*str != '\0') {
		*str = roCharToLower(*str);
		++str;
	}
}

void roStrToUpper(char* str)
{
	while(*str != '\0') {
		*str = (char)roCharToUpper(*str);
		++str;
	}
}

void roStrReverse(char *str, roSize len)
{
	if(len <= 1) return;
	for(roSize i=0; i<=(len - 1)/2; ++i)
		roSwap(str[i], str[len-1-i]);
}
