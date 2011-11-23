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

inline char* roStrStr(char* a, const char* b);
inline char* roStrrStr(char* a, const char* b);

inline char roToLower(char c);
inline char roToUpper(char c);

inline void roToLower(char* str);
inline void roToUpper(char* str);

inline void roStrReverse(char* str, roSize len);

#include "roStringUtility.inl"

#endif	// __roStringUtility_h__
