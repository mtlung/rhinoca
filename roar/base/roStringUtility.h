#ifndef __roStringUtility_h__
#define __roStringUtility_h__

#include "../platform/roCompiler.h"

inline roSize	roStrLen	(const char* str);

inline char*	roStrnCpy	(char* dst, const char* src, roSize n);

inline int		roStrCmp	(const char* s1, const char* s2);
inline int		roStrnCmp	(const char* s1, const char* s2, roSize n);
inline int		roStrCaseCmp(const char* s1, const char* s2);

inline char*	roStrChr	(char* sz, char ch);
inline char*	roStrrChr	(char* sz, char ch);

char*			roStrStr	(char* a, const char* b);
char*			roStrnStr	(char* a, roSize aLen, const char* b);
char*			roStrrStr	(char* a, const char* b);
char*			roStrrnStr	(char* a, roSize aLen, const char* b);

inline char		roToLower	(char c);
inline char		roToUpper	(char c);

void			roToLower	(char* str);
void			roToUpper	(char* str);

inline void		roStrReverse(char* str, roSize len);


// ----------------------------------------------------------------------
// String to different data types

bool		roStrTo			(const char* str, bool& ret);
bool		roStrTo			(const char* str, float& ret);
bool		roStrTo			(const char* str, double& ret);
bool		roStrTo			(const char* str, roInt8& ret);
bool		roStrTo			(const char* str, roInt16& ret);
bool		roStrTo			(const char* str, roInt32& ret);
bool		roStrTo			(const char* str, roInt64& ret);
bool		roStrTo			(const char* str, roUint8& ret);
bool		roStrTo			(const char* str, roUint16& ret);
bool		roStrTo			(const char* str, roUint32& ret);
bool		roStrTo			(const char* str, roUint64& ret);

bool		roStrToBool		(const char* str, bool defaultValue);
float		roStrToFloat	(const char* str, float defaultValue);
double		roStrToDouble	(const char* str, double defaultValue);
roInt8		roStrToInt8		(const char* str, roInt8 defaultValue);
roInt16		roStrToInt16	(const char* str, roInt16 defaultValue);
roInt32		roStrToInt32	(const char* str, roInt32 defaultValue);
roInt64		roStrToInt64	(const char* str, roInt64 defaultValue);
roUint8		roStrToUint8	(const char* str, roUint8 defaultValue);
roUint16	roStrToUint16	(const char* str, roUint16 defaultValue);
roUint32	roStrToUint32	(const char* str, roUint32 defaultValue);
roUint64	roStrToUint64	(const char* str, roUint64 defaultValue);


// ----------------------------------------------------------------------

bool roUtf8ToUtf16(roUint16* dest, roSize& destLen, const char* src, roSize maxSrcLen);
bool roUtf8ToUtf32(roUint32* dest, roSize& destLen, const char* src, roSize maxSrcLen);
bool roUtf16ToUtf8(char* dest, roSize& destLen, const roUint16* src, roSize maxSrcLen);

#include "roStringUtility.inl"

#endif	// __roStringUtility_h__
