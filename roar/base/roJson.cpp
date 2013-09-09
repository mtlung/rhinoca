#include "pch.h"
#include "roJson.h"
#include "roIOStream.h"
#include "roStringFormat.h"
#include "../Math/roMath.h"

namespace ro {

JsonParser::JsonParser() : _str(NULL), _event(Event::Undefined) {}

void JsonParser::parse(const roUtf8* source)
{
	_tmpStr = source;
	parseInplace(_tmpStr.c_str());
}

void JsonParser::parseInplace(roUtf8* source)
{
	if(source != _tmpStr.c_str()) {
		_tmpStr.clear();
		_tmpStr.condense();
	}

	_str = source;
	_it = source;
	_event = Event::Undefined;
	_stack.clear();
	_nextFunc = &JsonParser::_parseBeginObject;
}

void JsonParser::_skipWhiteSpace()
{
	while(*_it == '\x20' || *_it == '\x9' || *_it == '\xD' || *_it == '\xA')
		++_it;
}

bool JsonParser::_parseBeginObject()
{
	roAssert(*_it == '{');
	_event = Event::BeginObject;
	_stack.pushBack(_event);
	++_it;

	_skipWhiteSpace();
	if(*_it == '}')
		_nextFunc = &JsonParser::_parseEndObject;
	else
		_nextFunc = &JsonParser::_parseName;

	return true;
}

bool JsonParser::_parseEndObject()
{
	roAssert(*_it == '}');
	if(_stack.isEmpty() || _stack.back() != Event::BeginObject)
		return false;

	++_it;
	_stack.popBack();
	_event = Event::EndObject;

	if(_stack.isEmpty())
		_nextFunc = &JsonParser::_parseEnd;
	else
		return _valueEnd();

	return true;
}

bool JsonParser::_parseEnd()
{
	_event = Event::End;
	return true;
}

bool JsonParser::_parseBeginArray()
{
	roAssert(*_it == '[');

	++_it;
	_stack.pushBack(_event);
	_event = Event::BeginArray;

	_skipWhiteSpace();
	if(*_it == ']')
		_nextFunc = &JsonParser::_parseEndArray;
	else
		_nextFunc = &JsonParser::_parseValue;

	return true;
}

bool JsonParser::_parseEndArray()
{
	roAssert(*_it == ']');
	if(_stack.isEmpty() || _stack.back() != Event::BeginArray)
		return false;

	++_it;
	_stack.popBack();
	_event = Event::EndArray;

	return _valueEnd();
}

bool JsonParser::_parseValue()
{
	switch(*_it) {
	case '{':	return _parseBeginObject();
	case '}':	return _parseEndObject();
	case '[':	return _parseBeginArray();
	case ']':	return _parseEndArray();
	case '"':	return _parseString();
	case 'n':
	case 't':
	case 'f':	return _parseNullTrueFalse();
	default:	return _parseNumber();
	}
}

bool JsonParser::_valueEnd()
{
	_skipWhiteSpace();

	if(*_it == ',') {
		if(_stack.back() == Event::BeginObject)
			_nextFunc = &JsonParser::_parseName;
		else
			_nextFunc = &JsonParser::_parseValue;
		++_it;
	}
	else if(*_it == '}')
		_nextFunc = &JsonParser::_parseEndObject;
	else if(*_it == ']')
		_nextFunc = &JsonParser::_parseEndArray;
	else
		return false;

	return true;
}

bool JsonParser::_parseName()
{
	if(!_parseStringImpl())
		return false;

	_skipWhiteSpace();
	if(*_it != ':')
		return false;

	++_it;
	_event = Event::Name;
	_nextFunc = &JsonParser::_parseValue;

	return true;
}

bool JsonParser::_parseString()
{
	if(!_parseStringImpl())
		return false;

	_event = Event::String;
	return _valueEnd();
}

bool JsonParser::_parseStringImpl()
{
	if(*_it != '"')
		return false;

	++_it;
	roUtf8* first = _it;
	roUtf8* last = _it;

	while(*_it) {
		if(*_it == '\\') {
		}
		else if(*_it == '"') {
			*last = '\0';
			++_it;
			break;
		}
		else
			*last++ = *_it++;
	}

	_stringVal = first;
	return true;
}

bool JsonParser::_parseNumber()
{
	bool minus = false;
	if(*_it == '-') {
		minus = true;
		++_it;
	}
	else if(*_it == '+')
		++_it;

	// Take away any leading zero
	while(*_it == '0')
		++_it;

	// Try parse as int64
	roUint64 i = 0;
	bool useDouble = false;
	if(*_it >= '1' && *_it <= '9') {
		i = *_it - '0';
		++_it;

		if(minus) {
			while(*_it >= '0' && *_it <= '9') {
				if(i >= 922337203685477580uLL)	// 2^63 = 9223372036854775808
					if (i != 922337203685477580uLL || *_it > '8') {
						useDouble = true;
						break;
					}
				i = i * 10 + (*_it - '0');
				++_it;
			}
		}
		else {
			while(*_it >= '0' && *_it <= '9') {
				if(i >= 1844674407370955161uLL)	// 2^64 - 1 = 18446744073709551615
					if (i != 1844674407370955161uLL || *_it > '5') {
						useDouble = true;
						break;
					}
				i = i * 10 + (*_it - '0');
				++_it;
			}
		}
	}

	int expFrac = 0;
	double d = 0.0;
	if(*_it == '.') {
		if(!useDouble) {
			d = (double)i;
			useDouble = true;
		}
		++_it;

		if(*_it >= '0' && *_it <= '9') {
			d = d * 10 + (*_it - '0');
			++_it;
			--expFrac;
		}
		else
			return false;

		while(*_it >= '0' && *_it <= '9') {
			if(expFrac > -16) {
				d = d * 10 + (*_it - '0');
				--expFrac;
			}
			++_it;
		}
	}

	// Parse exp = e [ minus / plus ] 1*DIGIT
	int exp = 0;
	if(*_it == 'e' || *_it == 'E') {
		if(!useDouble) {
			d = (double)i;
			useDouble = true;
		}
		++_it;

		bool expMinus = false;
		if(*_it == '+')
			++_it;
		else if(*_it == '-') {
			++_it;
			expMinus = true;
		}

		if(*_it >= '0' && *_it <= '9') {
			exp = *_it - '0';
			++_it;
			while(*_it >= '0' && *_it <= '9') {
				exp = exp * 10 + (*_it - '0');
				++_it;
				if(exp > 308)
					return false;
			}
		}
		else
			return false;

		if (expMinus)
			exp = -exp;
	}

	// Finish parsing, call event according to the type of number.
	if(useDouble) {
		d *= roPow10d(exp + expFrac);
		_event = Event::Double;
		_doubleVal = (minus ? -d : d);
	}
	else {
		if(minus) {
			_int64Val = -((roInt64)i);
			_event = Event::Integer64;
		}
		else {
			_uint64Val = i;
			_event = Event::UInteger64;
		}
	}

	return _valueEnd();
}

bool JsonParser::_parseNullTrueFalse()
{
	if(_it[0] == 'n' && _it[1] == 'u' && _it[2] == 'l' && _it[3] == 'l') {
		_event = Event::Null;
		_it += 4;
	}
	else if(_it[0] == 't' && _it[1] == 'r' && _it[2] == 'u' && _it[3] == 'e') {
		_event = Event::Bool;
		_boolVal = true;
		_it += 4;
	}
	else if(_it[0] == 'f' && _it[1] == 'a' && _it[2] == 'l' && _it[3] == 's' && _it[4] == 'e') {
		_event = Event::Bool;
		_boolVal = false;
		_it += 5;
	}
	else
		return false;

	return _valueEnd();
}

JsonParser::Event::Enum JsonParser::nextEvent()
{
	if(_event == Event::End || _event == Event::Error)
		return _event;

	_skipWhiteSpace();
	if(!(this->*_nextFunc)())
		_event = Event::Error;

	return _event;
}

//
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
