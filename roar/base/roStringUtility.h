#ifndef __roStringUtility_h__
#define __roStringUtility_h__

#include "roCompiler.h"

inline char roCharToLower(char c);
inline char roCharToUpper(char c);

void roStrToLower(char* str);
void roStrToUpper(char* str);

void roStrReverse(char* str, roSize len);

#include "roStringUtility.inl"

#endif	// __roStringUtility_h__
