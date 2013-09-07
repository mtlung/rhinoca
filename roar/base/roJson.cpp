#include "pch.h"
#include "roJson.h"
#include "roIOStream.h"
#include "roStringFormat.h"

namespace ro {

JsonWriter::JsonWriter(OStream* stream)
	: _stream(stream)
	, _nestedLevel(0)
{}

void JsonWriter::setStream(OStream* stream)
{
	_stream = stream;
}

roStatus JsonWriter::flushStrBuf()
{
	if(!_stream) return roStatus::pointer_is_null;
	roStatus st = _stream->write(_strBuf.c_str(), _strBuf.size());
	if(!st) return st;
	_strBuf.clear();
	return st;
}

struct JsonString
{
	JsonString(const roUtf8* str) : buf((roByte*)str), bufLen(roStrLen(str)) {}
	JsonString(const roByte* b, roSize l) : buf(b), bufLen(l) {}
	const roByte* buf;
	roSize bufLen;
};

static void _strFormat_JsonString(String& str, JsonString* val, const roUtf8* options)
{
	if(!val) return;

	// Perform escape
	// TODO: Do escape only the option tell use do
	static const StaticArray<char, 16> hexDigits = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	static const StaticArray<char, 256> escape = {
	#define Z16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		//0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
		'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r', 'u', 'u',	// 00
		'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',	// 10
		  0,   0, '"',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 20
		Z16, Z16,																		// 30~4F
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0,	// 50
		Z16, Z16, Z16, Z16, Z16, Z16, Z16, Z16, Z16, Z16								// 60~FF
	#undef Z16
	};

	str.reserve(str.size() + val->bufLen);

	for(roSize i=0; i < val->bufLen; ++i) {
		roUtf8 c = val->buf[i];
		roUint8 esc = escape[(roUint8)c];
		if((sizeof(*val) == 1 || (unsigned)c < 256) && esc) {
			str.append('\\');
			str.append(esc);
			if(esc == 'u') {
				str.append('0', 2);
				str.append(hexDigits[(roUint8)c >> 4]);
				str.append(hexDigits[(roUint8)c & 0xF]);
			}
		}
		else
			str.append(c);
	}
}

static void* _strFormatFunc(const JsonString& val) {
	return (void*)_strFormat_JsonString;
}

template<typename T>
static roStatus writeNumbers(JsonWriter& writer, const roUtf8* name, T val)
{
	roAssert(writer._nestedLevel > 0);
	roStatus st = writer.flushStrBuf();
	if(!st) return st;
	return strFormat(writer._strBuf, "\"{}\":{},", JsonString(name), val);
}

roStatus JsonWriter::write(const roUtf8* name, roInt8 val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roInt16 val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roInt32 val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roInt64 val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roUint8 val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roUint16 val)	{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roUint32 val)	{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, roUint64 val)	{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, float val)		{ return writeNumbers(*this, name, val); }
roStatus JsonWriter::write(const roUtf8* name, double val)		{ return writeNumbers(*this, name, val); }

roStatus JsonWriter::write(const roUtf8* name, bool val)
{
	roAssert(_nestedLevel > 0);
	roStatus st = flushStrBuf();
	if(!st) return st;
	return strFormat(_strBuf, "\"{}\":\"{}\",", name, val ? "true" : "false");
}

roStatus JsonWriter::write(const roUtf8* name, const roUtf8* val)
{
	roAssert(_nestedLevel > 0);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\":\"{}\",", JsonString(name), JsonString(val));
}

roStatus JsonWriter::write(const roUtf8* name, const roByte* buf, roSize bufLen)
{
	roAssert(_nestedLevel > 0);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\":\"{}\",", JsonString(name), JsonString(buf, bufLen));
}

roStatus JsonWriter::beginObject(const roUtf8* name)
{
	roAssert(_nestedLevel > 0 || !name);
	roStatus st = flushStrBuf();
	if(!st) return st;

	if(name)
		st = strFormat(_strBuf, "\"{}\":{}", JsonString(name), "{");
	else
		st = _strBuf.assign("{");

	if(!st) return st;
	++_nestedLevel;
	return st;
}

roStatus JsonWriter::endObject()
{
	roAssert(_nestedLevel > 0);
	if(!_stream) return roStatus::pointer_is_null;

	if(!_strBuf.isEmpty() && _strBuf.back() == ',')
		_strBuf.decSize(1);

	roStatus st = flushStrBuf();
	if(!st) return st;

	if(_nestedLevel == 1)
		st = _stream->write("}\0");
	else
		st = _strBuf.assign("},");

	if(!st) return st;
	--_nestedLevel;
	return st;
}

roStatus JsonWriter::beginArray(const roUtf8* name)
{
	roAssert(_nestedLevel > 0);
	roStatus st = flushStrBuf();
	if(!st) return st;

	if(name)
		st = strFormat(_strBuf, "\"{}\":[", JsonString(name));
	else
		st = _strBuf.assign("[");

	if(!st) return st;
	++_nestedLevel;
	return st;
}

roStatus JsonWriter::endArray()
{
	roAssert(_nestedLevel > 0);
	if(!_stream) return roStatus::pointer_is_null;

	if(!_strBuf.isEmpty() && _strBuf.back() == ',')
		_strBuf.decSize(1);

	roStatus st = flushStrBuf();
	if(!st) return st;

	st = _strBuf.assign("],");
	if(!st) return st;

	--_nestedLevel;
	roAssert(_nestedLevel > 0);
	return st;
}

}   // namespace ro
