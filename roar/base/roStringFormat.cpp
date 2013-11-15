#include "pch.h"
#include "roStringFormat.h"
#include "roStringUtility.h"
#include "roArray.h"
#include <stdarg.h>
#include <stdio.h>

namespace ro {

void strPaddingLeft(String& str, roUtf8 c, roSize totalLen)
{
	if(str.size() >= totalLen)
		return;

	roSize movCount = str.size();
	roSize padCount = totalLen - movCount;

	str.resize(totalLen);
	roMemmov(str.c_str() + padCount, str.c_str(), movCount);

	for(roSize i=0; i<padCount; ++i)
		str[i] = c;
}

void strPaddingRight(String& str, roUtf8 c, roSize totalLen)
{
	if(str.size() >= totalLen)
		return;

	for(roSize i=totalLen - str.size(); i--;)
		str.append(c);
}

void _strFormat_int8(String& str, roInt8 val, const roUtf8* options)
{
	_strFormat_int64(str, val, options);
}

void _strFormat_int16(String& str, roInt16 val, const roUtf8* options)
{
	_strFormat_int64(str, val, options);
}

void _strFormat_int32(String& str, roInt32 val, const roUtf8* options)
{
	_strFormat_int64(str, val, options);
}

void _strFormat_int32ptr(String& str, roInt32* val, const roUtf8* options)
{
	if(val) _strFormat_int64(str, *val, options);
}

void _strFormat_int64(String& str, roInt64 val, const roUtf8* options)
{
	roSize bytes = roToString(NULL, 0, val, options);
	roAssert(bytes > 0);

	roSize padLen = 0;
	roUtf8 padding;
	if(sscanf(options, "pl%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);

	roSize offset = str.size();
	str.resize(str.size() + bytes);
	roToString(&str[offset], str.size() - offset + 1, val, options);

	if(sscanf(options, "pr%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);
}

void _strFormat_int64ptr(String& str, roInt64* val, const roUtf8* options)
{
	if(val) _strFormat_int64(str, *val, options);
}

void _strFormat_uint8(String& str, roUint8 val, const roUtf8* options)
{
	_strFormat_uint64(str, val, options);
}

void _strFormat_uint16(String& str, roUint16 val, const roUtf8* options)
{
	_strFormat_uint64(str, val, options);
}

void _strFormat_uint32(String& str, roUint32 val, const roUtf8* options)
{
	_strFormat_uint64(str, val, options);
}

void _strFormat_uint32ptr(String& str, roUint32* val, const roUtf8* options)
{
	if(val) _strFormat_uint64(str, *val, options);
}

void _strFormat_uint64(String& str, roUint64 val, const roUtf8* options)
{
	roSize bytes = roToString(NULL, 0, val, options);
	roAssert(bytes > 0);

	roSize padLen = 0;
	roUtf8 padding;
	if(sscanf(options, "pl%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);

	roSize offset = str.size();
	str.resize(str.size() + bytes);
	roToString(&str[offset], str.size() - offset + 1, val, options);

	if(sscanf(options, "pr%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);
}

void _strFormat_uint64ptr(String& str, roUint64* val, const roUtf8* options)
{
	if(val) _strFormat_uint64(str, *val, options);
}

void _strFormat_float(String& str, float val, const roUtf8* options)
{
	_strFormat_double(str, val, options);
}

void _strFormat_floatptr(String& str, float* val, const roUtf8* options)
{
	if(val) _strFormat_float(str, *val, options);
}

void _strFormat_double(String& str, double val, const roUtf8* options)
{
	roSize bytes = roToString(NULL, 0, val, options);
	roAssert(bytes > 0);

	roSize padLen = 0;
	roUtf8 padding;
	if(sscanf(options, "pl%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);

	roSize offset = str.size();
	str.resize(str.size() + bytes);
	roToString(&str[offset], str.size() - offset + 1, val, options);

	if(sscanf(options, "pr%u%c", &padLen, &padding) == 2 && padLen > bytes)
		str.append(padding, padLen - bytes);
}

void _strFormat_doubleptr(String& str, double* val, const roUtf8* options)
{
	if(val) _strFormat_double(str, *val, options);
}

void _strFormat_utf8(String& str, const roUtf8* val, const roUtf8* options)
{
	if(!val) return;

	roSize len = roStrLen(val);

	roSize padLen = 0;
	roUtf8 padding;
	if(sscanf(options, "pl%u%c", &padLen, &padding) == 2 && padLen > len)
		str.append(padding, padLen - len);

	str.append(val, len);

	if(sscanf(options, "pr%u%c", &padLen, &padding) == 2 && padLen > len)
		str.append(padding, padLen - len);
}

void _strFormat_str(String& str, const String* val, const roUtf8* options)
{
	if(!val) return;
	_strFormat_utf8(str, val->c_str(), options);
}

typedef bool (*FUNC)(String& str, const void* param, const roUtf8* options);

struct FormatArg
{
	FUNC func;
	const void* param;
};

roStatus _strFormatImpl(String& str, const roUtf8* format, FormatArg* args, roSize argCount)
{
	roSize argIdx = 0;

	// Scan for matching '{' '}'
	int i = 0;
	int pos = -1;
	for(const roUtf8* c = format; *c; ++c, ++i) {
		if(*c == '{') {
			if(pos > -1)
				str.append(format + pos, i-pos);
			pos = i;
		}

		if(pos > -1) {
			if(*c == '}') {
				if(argIdx < argCount) {
					TinyArray<roUtf8, 8> options;
					options.insert(0, format + pos + 1, format + i);
					options.pushBack('\0');
					args[argIdx].func(str, args[argIdx].param, options.bytePtr());
				}
				else {
					roAssert(false && "string format str accessing out of bound param");
					static const roUtf8 errorStr[] = "[invalid]";
					str.append(errorStr, roCountof(errorStr));
				}

				++argIdx;
				pos = -1;
			}
		}
		else
			str.append(*c);
	}

	return roStatus::ok;
}

roStatus _strFormat(String& str, const roUtf8* format, roSize argCount, ...)
{
	// Extract the function pointer and value pointer first
	TinyArray<FormatArg, 16> args;
	va_list vl;
	va_start(vl, argCount);

	for(roSize i=0; i<argCount; ++i) {
		FormatArg arg = { va_arg(vl, FUNC), va_arg(vl, void*) };
		args.pushBack(arg);
	}

	va_end(vl);

	return _strFormatImpl(str, format, args.typedPtr(), args.size());
}

}	// namespace ro
