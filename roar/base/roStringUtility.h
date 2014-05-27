#ifndef __roStringUtility_h__
#define __roStringUtility_h__

#include "roStatus.h"

inline roSize	roStrLen		(const roUtf8* str);
inline roSize	roStrLen		(const roUtf16* str);
inline roSize	roStrLen		(const roUtf8* str, roSize strLenMax);

inline char*	roStrnCpy		(char* dst, const char* src, roSize n);

inline int		roStrCmp		(const char* s1, const char* s2);
inline int		roStrnCmp		(const char* s1, const char* s2, roSize n);
inline int		roStrCaseCmp	(const char* s1, const char* s2);
inline int		roStrnCaseCmp	(const char* s1, const char* s2, roSize n);

inline char*	roStrChr		(char* sz, char ch);
inline char*	roStrrChr		(char* sz, char ch);
inline char*	roStrChrCase	(char* sz, char ch);
inline char*	roStrrChrCase	(char* sz, char ch);

char*			roStrStr		(char* a, const char* b);
char*			roStrStr		(char* a, const char* aEnd, const char* b);
char*			roStrnStr		(char* a, roSize aLen, const char* b);
char*			roStrrStr		(char* a, const char* b);
char*			roStrrnStr		(char* a, roSize aLen, const char* b);

char*			roStrStrCase	(char* a, const char* b);
char*			roStrStrCase	(char* a, const char* aEnd, const char* b);
char*			roStrnStrCase	(char* a, roSize aLen, const char* b);

inline char		roToLower		(char c);
inline char		roToUpper		(char c);

void			roToLower		(char* str);
void			roToUpper		(char* str);

bool			roIsDigit		(char ch);
bool			roIsAlpha		(char ch);

void			roStrReverse	(char* str, roSize len);


// ----------------------------------------------------------------------
// String to different data types

roStatus	roStrTo			(const char* str, bool& ret);
roStatus	roStrTo			(const char* str, float& ret);
roStatus	roStrTo			(const char* str, double& ret);
roStatus	roStrTo			(const char* str, roInt8& ret);
roStatus	roStrTo			(const char* str, roInt16& ret);
roStatus	roStrTo			(const char* str, roInt32& ret);
roStatus	roStrTo			(const char* str, roInt64& ret);
roStatus	roStrTo			(const char* str, roUint8& ret);
roStatus	roStrTo			(const char* str, roUint16& ret);
roStatus	roStrTo			(const char* str, roUint32& ret);
roStatus	roStrTo			(const char* str, roUint64& ret);

roStatus	roStrTo			(const char* str, roSize len, bool& ret);
roStatus	roStrTo			(const char* str, roSize len, float& ret);
roStatus	roStrTo			(const char* str, roSize len, double& ret);
roStatus	roStrTo			(const char* str, roSize len, roInt8& ret);
roStatus	roStrTo			(const char* str, roSize len, roInt16& ret);
roStatus	roStrTo			(const char* str, roSize len, roInt32& ret);
roStatus	roStrTo			(const char* str, roSize len, roInt64& ret);
roStatus	roStrTo			(const char* str, roSize len, roUint8& ret);
roStatus	roStrTo			(const char* str, roSize len, roUint16& ret);
roStatus	roStrTo			(const char* str, roSize len, roUint32& ret);
roStatus	roStrTo			(const char* str, roSize len, roUint64& ret);

roStatus	roStrTo			(const char* str, const char*& newPos, bool& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, float& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, double& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roInt8& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roInt16& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roInt32& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roInt64& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roUint8& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roUint16& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roUint32& ret);
roStatus	roStrTo			(const char* str, const char*& newPos, roUint64& ret);

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

bool		roStrToBool		(const char* str, roSize len, bool defaultValue);
float		roStrToFloat	(const char* str, roSize len, float defaultValue);
double		roStrToDouble	(const char* str, roSize len, double defaultValue);
roInt8		roStrToInt8		(const char* str, roSize len, roInt8 defaultValue);
roInt16		roStrToInt16	(const char* str, roSize len, roInt16 defaultValue);
roInt32		roStrToInt32	(const char* str, roSize len, roInt32 defaultValue);
roInt64		roStrToInt64	(const char* str, roSize len, roInt64 defaultValue);
roUint8		roStrToUint8	(const char* str, roSize len, roUint8 defaultValue);
roUint16	roStrToUint16	(const char* str, roSize len, roUint16 defaultValue);
roUint32	roStrToUint32	(const char* str, roSize len, roUint32 defaultValue);
roUint64	roStrToUint64	(const char* str, roSize len, roUint64 defaultValue);

roStatus	roHexStrTo		(const char* str, roUint8& ret);
roStatus	roHexStrTo		(const char* str, roUint16& ret);
roStatus	roHexStrTo		(const char* str, roUint32& ret);
roStatus	roHexStrTo		(const char* str, roUint64& ret);

roStatus	roHexStrTo		(const char* str, roSize len, roUint8& ret);
roStatus	roHexStrTo		(const char* str, roSize len, roUint16& ret);
roStatus	roHexStrTo		(const char* str, roSize len, roUint32& ret);
roStatus	roHexStrTo		(const char* str, roSize len, roUint64& ret);

roStatus	roHexStrTo		(const char* str, const char*& newPos, roUint8& ret);
roStatus	roHexStrTo		(const char* str, const char*& newPos, roUint16& ret);
roStatus	roHexStrTo		(const char* str, const char*& newPos, roUint32& ret);
roStatus	roHexStrTo		(const char* str, const char*& newPos, roUint64& ret);

roUint64	roHexStrToUint8	(const char* str, roUint64 defaultValue);
roUint64	roHexStrToUint16(const char* str, roUint64 defaultValue);
roUint64	roHexStrToUint32(const char* str, roUint64 defaultValue);
roUint64	roHexStrToUint64(const char* str, roUint64 defaultValue);

roUint8		roHexStrToUint8	(const char* str, roSize len, roUint8 defaultValue);
roUint16	roHexStrToUint16(const char* str, roSize len, roUint16 defaultValue);
roUint32	roHexStrToUint32(const char* str, roSize len, roUint32 defaultValue);
roUint64	roHexStrToUint64(const char* str, roSize len, roUint64 defaultValue);


// ----------------------------------------------------------------------
// Different data types to string

roSize		roToString		(char* str, roSize strBufSize, bool val, const char* option=NULL);	/// Returns number of bytes written, excluding '\0', returns 0 for failure
roSize		roToString		(char* str, roSize strBufSize, float val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, double val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roInt8 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roInt16 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roInt32 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roInt64 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roUint8 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roUint16 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roUint32 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, roUint64 val, const char* option=NULL);
roSize		roToString		(char* str, roSize strBufSize, const void* ptrVal, const char* option=NULL);	// Convert pointer into hex string

// ----------------------------------------------------------------------
// String encodings

int			roUtf8ToUtf16Char	(roUtf16& out, const roUtf8* src, roSize maxSrcLen);	/// Returns number of characters consumed in utf8 string, < 0 for any error
int			roUtf16ToUtf8		(roUtf8* utf8, roSize dstLen, roUtf16 utf16c);			/// Returns number of characters written to utf8 string, < 0 for any error
roStatus	roUtf8CountInUtf16	(roSize& outLen, const roUtf16* src);
roStatus	roUtf8CountInUtf16	(roSize& outLen, const roUtf16* src, roSize maxSrcLen);
roStatus	roUtf16CountInUtf8	(roSize& outLen, const roUtf8* src);
roStatus	roUtf16CountInUtf8	(roSize& outLen, const roUtf8* src, roSize maxSrcLen);

roStatus	roUtf8ToUtf16		(roUtf16* dest, roSize& dstLen, const roUtf8* src);
roStatus	roUtf8ToUtf16		(roUtf16* dest, roSize& dstLen, const roUtf8* src, roSize maxSrcLen);
roStatus	roUtf16ToUtf8		(roUtf8* dest, roSize& dstLen, const roUtf16* src);
roStatus	roUtf16ToUtf8		(roUtf8* dest, roSize& dstLen, const roUtf16* src, roSize maxSrcLen);

#include "roStringUtility.inl"

#endif	// __roStringUtility_h__
