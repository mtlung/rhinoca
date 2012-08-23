#ifndef __roString_Format_h__
#define __roString_Format_h__

#include "roString.h"

namespace ro {

void strPaddingLeft			(String& str, roUtf8 c, roSize totalLen);
void strPaddingRight		(String& str, roUtf8 c, roSize totalLen);

void _strFormat_float		(String& str, float val, const roUtf8* options);
void _strFormat_double		(String& str, double val, const roUtf8* options);
void _strFormat_int8		(String& str, roInt8 val, const roUtf8* options);
void _strFormat_int16		(String& str, roInt16 val, const roUtf8* options);
void _strFormat_int32		(String& str, roInt32 val, const roUtf8* options);
void _strFormat_int64		(String& str, roInt64 val, const roUtf8* options);
void _strFormat_uint8		(String& str, roUint8 val, const roUtf8* options);
void _strFormat_uint16		(String& str, roUint16 val, const roUtf8* options);
void _strFormat_uint32		(String& str, roUint32 val, const roUtf8* options);
void _strFormat_uint64		(String& str, roUint64 val, const roUtf8* options);
void _strFormat_floatptr	(String& str, float* val, const roUtf8* options);
void _strFormat_doubleptr	(String& str, double* val, const roUtf8* options);
void _strFormat_int32ptr	(String& str, roInt32* val, const roUtf8* options);
void _strFormat_int64ptr	(String& str, roInt64* val, const roUtf8* options);
void _strFormat_uint32ptr	(String& str, roUint32* val, const roUtf8* options);
void _strFormat_uint64ptr	(String& str, roUint64* val, const roUtf8* options);
void _strFormat_utf8		(String& str, const roUtf8* val, const roUtf8* options);
void _strFormat_str			(String& str, const String* val, const roUtf8* options);

inline void* _strFormatFunc(roInt8 val) {
	return (void*)_strFormat_int8;
}
inline void* _strFormatFunc(roInt16 val) {
	return (void*)_strFormat_int16;
}
inline void* _strFormatFunc(roInt32 val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_int32ptr : (void*)_strFormat_int32;
}
inline void* _strFormatFunc(roInt64 val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_int64ptr : (void*)_strFormat_int64;
}
inline void* _strFormatFunc(roUint8 val) {
	return (void*)_strFormat_uint8;
}
inline void* _strFormatFunc(roUint16 val) {
	return (void*)_strFormat_uint16;
}
inline void* _strFormatFunc(roUint32 val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_uint32ptr : (void*)_strFormat_uint32;
}
inline void* _strFormatFunc(roUint64 val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_uint64ptr : (void*)_strFormat_uint64;
}
inline void* _strFormatFunc(float val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_floatptr : (void*)_strFormat_float;
}
inline void* _strFormatFunc(double val) {
	return sizeof(val) != sizeof(void*) ? (void*)_strFormat_doubleptr : (void*)_strFormat_double;
}
inline void* _strFormatFunc(const roUtf8* val) {
	return (void*)_strFormat_utf8;
}
inline void* _strFormatFunc(const String& val) {
	return (void*)_strFormat_str;
}

inline const void* _strFormatArg(roInt8 val) {
	return (void*)val;
}
inline const void* _strFormatArg(roInt16 val) {
	return (void*)val;
}
inline const void* _strFormatArg(const roInt32& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)val;
}
inline const void* _strFormatArg(const roInt64& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)val;
}
inline const void* _strFormatArg(roUint8 val) {
	return (void*)val;
}
inline const void* _strFormatArg(roUint16 val) {
	return (void*)val;
}
inline const void* _strFormatArg(const roUint32& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)val;
}
inline const void* _strFormatArg(const roUint64& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)val;
}
inline const void* _strFormatArg(const float& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)(*(int*)(&val));
}
inline const void* _strFormatArg(const double& val) {
	return sizeof(val) != sizeof(void*) ? &val : (void*)(*(int*)(&val));
}
inline const void* _strFormatArg(roUtf8* val) {
	return (void*)val;
}
inline const void* _strFormatArg(const roUtf8* val) {
	return (void*)val;
}
template <class T>
inline const void* _strFormatArg(const T& val) {
	return (void*)&val;
}

#define _EXPAND(x) _strFormatFunc(x), _strFormatArg(x)

/// C# like string formatting
/// strFormat(str, "{}, {}, {}", 1, 1.2, "hello"); -> "1, 1.2, Hello"
///
/// Option string can be specified inside {}
/// strFormat(str, "{lp5 }", 123); -> "  123"	// Pad on left with ' ' until len = 5
/// strFormat(str, "{rp5-}", 123); -> "123--"	// Pad on right with '-' until len = 5
roStatus _strFormat(String& str, const roUtf8* format, roSize argCount, ...);

template<class T1>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1) {
	return _strFormat(str, format, 1, _EXPAND(t1));
}

template<class T1, class T2>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2) {
	return _strFormat(str, format, 2, _EXPAND(t1), _EXPAND(t2));
}

template<class T1, class T2, class T3>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3) {
	return _strFormat(str, format, 3, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3));
}

template<class T1, class T2, class T3, class T4>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4) {
	return _strFormat(str, format, 4, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4));
}

template<class T1, class T2, class T3, class T4, class T5>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5) {
	return _strFormat(str, format, 5, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5));
}

template<class T1, class T2, class T3, class T4, class T5, class T6>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6) {
	return _strFormat(str, format, 6, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5), _EXPAND(t6));
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6, const T7& t7) {
	return _strFormat(str, format, 7, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5), _EXPAND(t6), _EXPAND(t7));
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6, const T7& t7, const T8& t8) {
	return _strFormat(str, format, 8, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5), _EXPAND(t6), _EXPAND(t7), _EXPAND(t8));
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6, const T7& t7, const T8& t8, const T9& t9) {
	return _strFormat(str, format, 9, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5), _EXPAND(t6), _EXPAND(t7), _EXPAND(t8), _EXPAND(t9));
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
roStatus strFormat(String& str, const roUtf8* format, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6, const T7& t7, const T8& t8, const T9& t9, const T10& t10) {
	return _strFormat(str, format, 10, _EXPAND(t1), _EXPAND(t2), _EXPAND(t3), _EXPAND(t4), _EXPAND(t5), _EXPAND(t6), _EXPAND(t7), _EXPAND(t8), _EXPAND(t9), _EXPAND(t10));
}

#undef _EXPAND

}	// namespace

#endif	// __roString_Format_h__