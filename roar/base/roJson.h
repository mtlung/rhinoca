#ifndef __roJson_h__
#define __roJson_h__

#include "roArray.h"
#include "roString.h"
#include "roTypeCast.h"
#include "roNonCopyable.h"

namespace ro {

struct OStream;

// See RFC4627:
// http://tools.ietf.org/html/rfc4627
struct JsonParser : private NonCopyable
{
	struct Event { enum Enum {
		End = -1,
		Error = 0,
		Null,
		BeginObject,	EndObject,
		BeginArray,		EndArray,
		Name,
		Bool,
		String,
		Integer64,		UInteger64,
		Double,
		Undefined
	}; };	// Event

	JsonParser();

	void parse(const roUtf8* source);

	// This function will alter the source string, but will not own the string.
	void parseInplace(roUtf8* source);

	Event::Enum		nextEvent();
	Event::Enum		currentEvent()			{ return _event; }
	Event::Enum		getCurrentProcessNext() { Event::Enum e = _event; nextEvent(); return e; }

	bool			getBool()				{ roAssert(_event == Event::Bool); return _boolVal; }
	const roUtf8*	getName()				{ roAssert(_event == Event::Name); return _stringVal; }
	const roUtf8*	getString()				{ roAssert(_event == Event::String); return _stringVal; }
	roSize			getStringLen()			{ roAssert(_event == Event::String); return _stringValLen; }
	roInt64			getInt64()				{ roAssert(_event == Event::Integer64); return _int64Val; }
	roUint64		getUint64()				{ roAssert(_event == Event::UInteger64); return _uint64Val; }
	double			getDouble()				{ roAssert(_event == Event::Double); return _doubleVal; }

	template<typename T>
	roStatus		getNumber(T& val);
	bool			isNumber(Event::Enum e);

	roSize			getObjectMemberCount()	{ roAssert(_event == Event::EndObject); return _MemberCount; }
	roSize			getArrayElementCount()	{ roAssert(_event == Event::EndArray); return _ElementCount; }

	const roUtf8*	getErrorMessage()		{ roAssert(_event == Event::Error); return _errStr.c_str(); }

// Private
	void			_skipWhiteSpace();
	bool			_unInitializedCallback();
	bool			_parseRoot();
	bool			_parseBeginObject();
	bool			_parseEndObject();
	bool			_parseEnd();
	bool			_parseBeginArray();
	bool			_parseEndArray();
	bool			_parseValue();
	bool			_valueEnd();
	bool			_parseName();
	bool			_parseString();
	bool			_parseStringImpl();
	bool			_parseNumber();
	bool			_parseNullTrueFalse();
	bool			_onError(roStatus st, const roUtf8* errMsg);

	roUtf8*		_str;
	roUtf8*		_it;
	String		_tmpStr;
	String		_errStr;
	roStatus	_status;
	Event::Enum	_event;

	struct _StackElement { Event::Enum event; roSize elementCount; };
	Array<_StackElement> _stack;

	bool (JsonParser::*_nextFunc)();

	bool		_boolVal;
	roUtf8*		_stringVal;		roSize		_stringValLen;
	roInt64		_int64Val;		roUint64	_uint64Val;
	double		_doubleVal;
	roSize		_MemberCount;	roSize		_ElementCount;
};	// JsonParser

// A simple structure for Dom style parsing
struct JsonValue
{
	JsonValue* parent;
	JsonValue* nextSibling;
	JsonValue* firstChild;
	JsonValue* lastChild;

	const roUtf8* name;					// Null for non object member
	union
	{
		bool			boolVal;
		const roUtf8*	stringVal;
		roInt64			int64Val;
		roUint64		uint64Val;
		double			doubleVal;
		roSize			arraySize;		// For type == BeginArray
		roSize			memberCount;	// For type == BeginObject
	};

	JsonParser::Event::Enum type;

	template<typename T>
	roStatus	getNumber(T& val);
	bool		isNumber();
};	// JsonValue

struct BlockAllocator;

// Dom style parsing
JsonValue* jsonParseInplace(roUtf8* source, BlockAllocator& allocator, String* errorStr=NULL);

struct JsonWriter : private NonCopyable
{
	JsonWriter(OStream* stream=NULL);
	~JsonWriter();

	void setStream(OStream* stream);

	void beginDocument();
	void endDocument();
	void Reset();

// For array
	roStatus beginArray(const roUtf8* name=NULL);
	roStatus endArray();
	roStatus writeNull();
	roStatus write(bool val);
	roStatus write(roInt8 val);
	roStatus write(roInt16 val);
	roStatus write(roInt32 val);
	roStatus write(roInt64 val);
	roStatus write(roUint8 val);
	roStatus write(roUint16 val);
	roStatus write(roUint32 val);
	roStatus write(roUint64 val);
	roStatus write(float val);
	roStatus write(double val);
	roStatus write(const roUtf8* val);
	roStatus write(const RangedString& val);
	roStatus write(const roByte* buf, roSize bufLen);

// For object
	roStatus beginObject(const roUtf8* name=NULL);
	roStatus endObject();
	roStatus writeNull(const roUtf8* name);
	roStatus write(const roUtf8* name, bool val);
	roStatus write(const roUtf8* name, roInt8 val);
	roStatus write(const roUtf8* name, roInt16 val);
	roStatus write(const roUtf8* name, roInt32 val);
	roStatus write(const roUtf8* name, roInt64 val);
	roStatus write(const roUtf8* name, roUint8 val);
	roStatus write(const roUtf8* name, roUint16 val);
	roStatus write(const roUtf8* name, roUint32 val);
	roStatus write(const roUtf8* name, roUint64 val);
	roStatus write(const roUtf8* name, float val);
	roStatus write(const roUtf8* name, double val);
	roStatus write(const roUtf8* name, const roUtf8* val);
	roStatus write(const roUtf8* name, const RangedString& val);
	roStatus write(const roUtf8* name, const roByte* buf, roSize bufLen);

// Private
	roStatus flushStrBuf();
	OStream* _stream;
	bool _beginDocument;
	String _strBuf;
	enum State { _inObject, _inArray };
	Array<State> _stateStack;
};  // JsonWriter


template<typename T>
roStatus JsonParser::getNumber(T& val)
{
	if(_event == Event::Integer64)
		return roSafeAssign(val, getInt64());

	if(_event == Event::UInteger64)
		return roSafeAssign(val, getUint64());

	if(_event == Event::Double)
		return roSafeAssign(val, getDouble());

	return roStatus::type_mismatch;
}

template<typename T>
roStatus JsonValue::getNumber(T& val)
{
	if(type == JsonParser::Event::Integer64)
		return roSafeAssign(val, int64Val);

	if(type == JsonParser::Event::UInteger64)
		return roSafeAssign(val, uint64Val);

	if(type == JsonParser::Event::Double)
		return roSafeAssign(val, doubleVal);

	return roStatus::type_mismatch;
}

}   // namespace ro

#endif	// __roJson_h__
