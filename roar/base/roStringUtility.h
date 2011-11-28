#ifndef __roStringUtility_h__
#define __roStringUtility_h__

#include "../platform/roCompiler.h"

inline roSize roStrLen(const char* str);

inline char* roStrnCpy(char* dst, const char* src, roSize n);

inline int roStrCmp(const char* s1, const char* s2);
inline int roStrnCmp(const char* s1, const char* s2, roSize n);
inline int roStrCaseCmp(const char* s1, const char* s2);

inline char* roStrChr(char* sz, char ch);
inline char* roStrrChr(char* sz, char ch);

char* roStrStr(char* a, const char* b);
char* roStrrStr(char* a, const char* b);

inline char roToLower(char c);
inline char roToUpper(char c);

void roToLower(char* str);
void roToUpper(char* str);

inline void roStrReverse(char* str, roSize len);

bool roStrToBool(const char* str, bool defaultValue);

// ----------------------------------------------------------------------

bool roUtf8ToUtf16(roUint16* dest, roSize& destLen, const char* src, roSize maxSrcLen);
bool roUtf8ToUtf32(roUint32* dest, roSize& destLen, const char* src, roSize maxSrcLen);
bool roUtf16ToUtf8(char* dest, roSize& destLen, const roUint16* src, roSize maxSrcLen);

#include "roStringUtility.inl"

#endif	// __roStringUtility_h__
