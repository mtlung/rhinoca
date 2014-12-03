#include "pch.h"
#include "roJson.h"
#include "roIOStream.h"
#include "roStringFormat.h"
#include "roBlockAllocator.h"
#include "../Math/roMath.h"

namespace ro {

JsonParser::JsonParser()
	: _str(""), _it(""), _event(Event::Undefined), _nextFunc(&JsonParser::_unInitializedCallback)
{}

void JsonParser::parse(const roUtf8* source)
{
	_tmpStr = source;
	parseInplace(_tmpStr.c_str());
}

void JsonParser::parseInplace(roUtf8* source)
{
	if(!source)
		source = "";

	if(source != _tmpStr.c_str()) {
		_tmpStr.clear();
		_tmpStr.condense();
	}

	_str = source;
	_it = source;
	_event = Event::Undefined;
	_stack.clear();
	_nextFunc = &JsonParser::_parseRoot;
}

void JsonParser::_skipWhiteSpace()
{
	while(*_it == '\x20' || *_it == '\x9' || *_it == '\xD' || *_it == '\xA')
		++_it;
}

bool JsonParser::_unInitializedCallback()
{
	return _onError(roStatus::not_initialized, "Please call parse()");
}

bool JsonParser::_onError(roStatus st, const roUtf8* errMsg)
{
	_status = st;
	_errStr = errMsg;
	return false;
}

bool JsonParser::_parseRoot()
{
	_skipWhiteSpace();
	switch(*_it) {
		case '{': return _parseBeginObject();
		case '[': return _parseBeginArray();
		default: return _onError(roStatus::json_parse_error, "Expecting object or array as root");
	}
}

bool JsonParser::_parseBeginObject()
{
	roAssert(*_it == '{');
	_event = Event::BeginObject;
	_StackElement s = { _event, 0 };
	_stack.pushBack(s);
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
	if(_stack.isEmpty() || _stack.back().event != Event::BeginObject)
		return _onError(roStatus::json_parse_error, "unexpected '}'");

	++_it;
	_MemberCount = _stack.back().elementCount;
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
	_event = Event::BeginArray;
	_StackElement s = { _event, 0 };
	_stack.pushBack(s);

	_skipWhiteSpace();
	_nextFunc = &JsonParser::_parseValue;

	return true;
}

bool JsonParser::_parseEndArray()
{
	roAssert(*_it == ']');
	if(_stack.isEmpty() || _stack.back().event != Event::BeginArray)
		return _onError(roStatus::json_parse_error, "unexpected ']'");

	++_it;
	_ElementCount = _stack.back().elementCount;
	_stack.popBack();
	_event = Event::EndArray;

	if(_stack.isEmpty())
		_nextFunc = &JsonParser::_parseEnd;
	else
		return _valueEnd();

	return true;
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
		if(_stack.back().event == Event::BeginObject)
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
		return _onError(roStatus::json_parse_error, "Expecting ',}]'");

	_stack.back().elementCount++;
	return true;
}

bool JsonParser::_parseName()
{
	if(!_parseStringImpl())
		return false;

	_skipWhiteSpace();
	if(*_it != ':')
		return _onError(roStatus::json_parse_error, "Expecting ':'");

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

static bool parseHex4(roUtf8* it, unsigned& codepoint)
{
	codepoint = 0;
	for(roSize i = 0; i<4; ++i) {
		roUtf8 c = *it;
		++it;
		codepoint <<= 4;
		codepoint += c;
		if (c >= '0' && c <= '9')
			codepoint -= '0';
		else if (c >= 'A' && c <= 'F')
			codepoint -= 'A' - 10;
		else if (c >= 'a' && c <= 'f')
			codepoint -= 'a' - 10;
		else 
			return false;
	}

	return true;
}

bool JsonParser::_parseStringImpl()
{
	if(*_it != '"')
		return _onError(roStatus::json_parse_error, "Expecting '\"'");

	++_it;
	roUtf8* first = _it;
	roUtf8* last = _it;

	while(*_it) {
		if((roUint8)*_it < '\x20')	// Control characters not allowed
			return false;
		else if(*_it == '\\') {
			switch(_it[1]) {
			case '"':	*last = '"';	break;
			case '\\':	*last = '\\';	break;
			case '/':	*last = '/';	break;
			case 'b':	*last = '\b';	break;
			case 'f':	*last = '\f';	break;
			case 'n':	*last = '\n';	break;
			case 'r':	*last = '\r';	break;
			case 't':	*last = '\t';	break;
			case 'u':
				unsigned codepoint;
				if(!parseHex4(_it + 2, codepoint))
					return false;

				// Reference:
				// http://www.russellcottrell.com/greek/utilities/SurrogatePairCalculator.htm
				if(codepoint >= 0xD800 && codepoint <= 0xDBFF) {	// Detect hight-surrogate code point
					_it += 6;
					if(_it[0] != '\\' || _it[1] != 'u')				// Expecting low-surrogate code point
						return false;
					unsigned codepoint2;
					if(!parseHex4(_it + 2, codepoint2))
						return false;
					if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
						return false;

					codepoint = (((codepoint - 0xD800) << 10) | (codepoint2 - 0xDC00)) + 0x10000;
				}

				if(codepoint <= 0x7F)
					*last = (roUtf8)codepoint;
				else if(codepoint <= 0x7FF) {
					*last++ = (roUtf8)(0xC0 | (codepoint >> 6));
					*last   = (roUtf8)(0x80 | (codepoint & 0x3F));
				}
				else if(codepoint <= 0xFFFF) {
					*last++ = (roUtf8)(0xE0 | (codepoint >> 12));
					*last++ = (roUtf8)(0x80 | ((codepoint >> 6) & 0x3F));
					*last   = (roUtf8)(0x80 | (codepoint & 0x3F));
				}
				else {
					roAssert(codepoint <= 0x10FFFF);
					*last++ = (roUtf8)(0xF0 | (codepoint >> 18));
					*last++ = (roUtf8)(0x80 | ((codepoint >> 12) & 0x3F));
					*last++ = (roUtf8)(0x80 | ((codepoint >> 6) & 0x3F));
					*last   = (roUtf8)(0x80 | (codepoint & 0x3F));
				}
				_it += 4;
				break;
			default:
				return false;
			}
			++last;
			_it += 2;
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
	_stringValLen = last - first;
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

	// Force double if integer overflow occur
	double d = 0.0;
	if (useDouble) {
		d = (double)i;
		while(*_it >= '0' && *_it <= '9') {
			if(d >= 1E307)	// Even double cannot store the number 
				return false;
			d = d * 10 + (*_it - '0');
			++_it;
		}
	}

	int expFrac = 0;
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
		return Event::Error;

	_skipWhiteSpace();
	if(!(this->*_nextFunc)())
		_event = Event::Error;

	return _event;
}

bool JsonParser::isNumber(Event::Enum e)
{
	return e == Event::Integer64 || e == Event::UInteger64 || e == Event::Double;
}

bool JsonValue::isNumber()
{
	typedef JsonParser::Event Event;
	return type == Event::Integer64 || type == Event::UInteger64 || type == Event::Double;
}

static JsonValue* _parse(JsonParser& parser, JsonValue* parentNode, BlockAllocator& allocator)
{
	typedef JsonParser::Event Event;
	JsonValue* newValue = NULL;

	while(true) {
		JsonParser::Event::Enum e = parser.nextEvent();
		if(e == JsonParser::Event::EndObject) return newValue;
		if(e == JsonParser::Event::EndArray) return newValue;
		if(e == JsonParser::Event::End) return newValue;
		if(e == JsonParser::Event::Error) return NULL;

		newValue = allocator.malloc(sizeof(JsonValue)).cast<JsonValue>();
		if(!newValue) {
			parser._event = JsonParser::Event::Error;
			parser._errStr = "Out of memory";
			return NULL;
		}

		roMemZeroStruct(*newValue);
		if(e == Event::Name) {
			newValue->name = parser.getName();
			e = parser.nextEvent();
		}

		newValue->type = e;
		newValue->parent = parentNode;
		if(!parentNode->firstChild)
			parentNode->firstChild = newValue;
		else
			parentNode->lastChild->nextSibling = newValue;
		parentNode->lastChild = newValue;

		switch(e) {
		case Event::Null:
			break;
		case Event::Bool:
			newValue->boolVal = parser.getBool();
			break;
		case Event::String:
			newValue->stringVal = parser.getString();
			break;
		case Event::Integer64 :
			newValue->int64Val = parser.getInt64();
			break;
		case Event::UInteger64 :
			newValue->uint64Val = parser.getUint64();
			break;
		case Event::Double :
			newValue->doubleVal = parser.getDouble();
			break;

		case Event::BeginObject:
			_parse(parser, newValue, allocator);
			break;
		case Event::BeginArray:
			_parse(parser, newValue, allocator);
			break;

		default:
			roAssert(false);
			return NULL;
		}

		++parentNode->memberCount;
	}

	return newValue;
}

JsonValue* jsonParseInplace(roUtf8* source, BlockAllocator& allocator, String* errorStr)
{
	JsonParser parser;
	parser.parseInplace(source);

	JsonValue* ret = NULL;
	JsonValue* dummyRoot = allocator.malloc(sizeof(JsonValue)).cast<JsonValue>();

	if(dummyRoot) {
		roMemZeroStruct(*dummyRoot);
		ret = _parse(parser, dummyRoot, allocator);
	}
	else {
		parser._event = JsonParser::Event::Error;
		parser._errStr = "Out of memory";
	}

	if(errorStr && parser.currentEvent() == JsonParser::Event::Error)
		*errorStr = parser.getErrorMessage();

	if(ret)
		ret->parent = NULL;

	return ret;
}

//
JsonWriter::JsonWriter(OStream* stream)
	: _stream(stream)
	, _beginDocument(false)
{}

JsonWriter::~JsonWriter()
{
	roAssert(!_beginDocument);
}

void JsonWriter::setStream(OStream* stream)
{
	_stream = stream;
}

void JsonWriter::beginDocument()
{
	roAssert(_stream);
	_beginDocument = true;
	_strBuf.clear();
}

void JsonWriter::endDocument()
{
	_beginDocument = false;
	roAssert(_stateStack.size() == 0);
	roAssert(_strBuf.isEmpty());
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
	JsonString(const RangedString& str) : buf((const roByte*)str.begin), bufLen(str.size()) {}
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

roStatus JsonWriter::beginObject(const roUtf8* name)
{
	roStatus st = flushStrBuf();
	if(!st) return st;

	if(name) {
		roAssert(_stateStack.back() == JsonWriter::_inObject);
		st = strFormat(_strBuf, "\"{}\":{}", JsonString(name), "{");
	}
	else
		st = _strBuf.assign("{");

	if(!st) return st;
	_stateStack.pushBack(_inObject);
	return st;
}

roStatus JsonWriter::endObject()
{
	roAssert(_stateStack.back() == _inObject);
	if(!_stream) return roStatus::pointer_is_null;

	if(!_strBuf.isEmpty() && _strBuf.back() == ',')
		_strBuf.decSize(1);

	roStatus st = flushStrBuf();
	if(!st) return st;

	if(_stateStack.size() == 1)
		st = _stream->write("}\0");
	else
		st = _strBuf.assign("},");

	if(!st) return st;
	_stateStack.popBack();
	return st;
}

roStatus JsonWriter::writeNull(const roUtf8* name)
{
	roAssert(_stateStack.back() == _inObject);
	roStatus st = flushStrBuf();
	if(!st) return st;
	return strFormat(_strBuf, "\"{}\":null,", JsonString(name));
}

template<typename T>
static roStatus writeNumbers(JsonWriter& writer, const roUtf8* name, T val)
{
	roAssert(writer._stateStack.back() == JsonWriter::_inObject);
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
	roAssert(_stateStack.back() == _inObject);
	roStatus st = flushStrBuf();
	if(!st) return st;
	return strFormat(_strBuf, "\"{}\":{},", name, val ? "true" : "false");
}

roStatus JsonWriter::write(const roUtf8* name, const roUtf8* val)
{
	roAssert(_stateStack.back() == _inObject);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\":\"{}\",", JsonString(name), JsonString(val));
}

roStatus JsonWriter::write(const roUtf8* name, const RangedString& val)
{
	roAssert(_stateStack.back() == _inObject);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\":\"{}\",", JsonString(name), JsonString(val));
}

roStatus JsonWriter::write(const roUtf8* name, const roByte* buf, roSize bufLen)
{
	roAssert(_stateStack.back() == _inObject);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\":\"{}\",", JsonString(name), JsonString(buf, bufLen));
}

roStatus JsonWriter::beginArray(const roUtf8* name)
{
	roStatus st = flushStrBuf();
	if(!st) return st;

	if(name) {
		roAssert(_stateStack.back() == _inObject);
		st = strFormat(_strBuf, "\"{}\":[", JsonString(name));
	}
	else
		st = _strBuf.assign("[");

	if(!st) return st;
	_stateStack.pushBack(_inArray);
	return st;
}

roStatus JsonWriter::endArray()
{
	roAssert(_stateStack.back() == _inArray);
	if(!_stream) return roStatus::pointer_is_null;

	if(!_strBuf.isEmpty() && _strBuf.back() == ',')
		_strBuf.decSize(1);

	roStatus st = flushStrBuf();
	if(!st) return st;

	st = _strBuf.assign("],");
	if(!st) return st;

	_stateStack.popBack();
	return st;
}

roStatus JsonWriter::writeNull()
{
	roAssert(_stateStack.back() == _inArray);
	roStatus st = flushStrBuf();
	if(!st) return st;
	return _strBuf.append("null,");
}

template<typename T>
static roStatus writeNumbers(JsonWriter& writer, T val)
{
	roAssert(writer._stateStack.back() == JsonWriter::_inArray);
	roStatus st = writer.flushStrBuf();
	if(!st) return st;
	return strFormat(writer._strBuf, "{},", val);
}

roStatus JsonWriter::write(roInt8 val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roInt16 val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roInt32 val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roInt64 val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roUint8 val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roUint16 val)	{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roUint32 val)	{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(roUint64 val)	{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(float val)		{ return writeNumbers(*this, val); }
roStatus JsonWriter::write(double val)		{ return writeNumbers(*this, val); }

roStatus JsonWriter::write(bool val)
{
	roAssert(_stateStack.back() == _inArray);
	roStatus st = flushStrBuf();
	if(!st) return st;
	return strFormat(_strBuf, "{},", val ? "true" : "false");
}

roStatus JsonWriter::write(const roUtf8* val)
{
	roAssert(_stateStack.back() == _inArray);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\",", JsonString(val));
}

roStatus JsonWriter::write(const RangedString& val)
{
	roAssert(_stateStack.back() == _inArray);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\",", JsonString(val));
}

roStatus JsonWriter::write(const roByte* buf, roSize bufLen)
{
	roAssert(_stateStack.back() == _inArray);
	roStatus st = flushStrBuf();
	if(!st) return st;

	return strFormat(_strBuf, "\"{}\",", JsonString(buf, bufLen));
}

}   // namespace ro
