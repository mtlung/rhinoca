#include "pch.h"
#include "roStringUtility.h"
#include "roUtility.h"

void roStrReverse(char *str, roSize len)
{
	if(len <= 1) return;
	for(roSize i=0; i<=(len - 1)/2; ++i)
		roSwap(str[i], str[len-1-i]);
}
