#ifndef __roJson_h__
#define __roJson_h__

#include "roArray.h"
#include "roString.h"
#include "roNonCopyable.h"

namespace ro {

struct OStream;

struct JsonParser : private NonCopyable
{
	struct Event { enum Enum {
		Error = 0,
		Null,
		BeginObject,	EndObject,
		BeginArray,		EndArray,
		Name,
		Bool,
		String,
		Integer32,		UInteger32,
		Integer64,		UInteger64,
		Double,
		End,
		Undefined
	}; };	// Event

	JsonParser();

	void parse(const roUtf8* source);

	// This function will alter the source string, but will not own the string.
	void parseInplace(roUtf8* source);

	Event::Enum		nextEvent();

	bool			getBool()		{ roAssert(_event == Event::Bool); return _boolVal; }
	const roUtf8*	getName()		{ roAssert(_event == Event::Name); return _stringVal; }
	const roUtf8*	getString()		{ roAssert(_event == Event::String); return _stringVal; }
	roInt32			getInt32()		{ roAssert(_event == Event::Integer32); return _int32Val; }
	roUint32		getUint32()		{ roAssert(_event == Event::UInteger32); return _uint32Val; }
	roInt64			getInt64()		{ roAssert(_event == Event::Integer64); return _int64Val; }
	roUint64		getUint64()		{ roAssert(_event == Event::UInteger64); return _uint64Val; }
	double			getDouble()		{ roAssert(_event == Event::Double); return _doubleVal; }

	roSize			getObjectMemberCount()	{ roAssert(_event == Event::EndObject); return _MemberCount; }
	roSize			getArrayElementCount()	{ roAssert(_event == Event::EndArray); return _ElementCount; }

// Private
	void			_skipWhiteSpace();
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

	roUtf8*		_str;
	roUtf8*		_it;
	String		_tmpStr;
	Event::Enum	_event;
	Array<Event::Enum> _stack;

	bool (JsonParser::*_nextFunc)();

	bool		_boolVal;
	roUtf8*		_stringVal;
	roInt32		_int32Val;		roUint32	_uint32Val;
	roInt64		_int64Val;		roUint64	_uint64Val;
	double		_doubleVal;
	roSize		_MemberCount;	roSize		_ElementCount;
};	// JsonParser

struct JsonWriter : private NonCopyable
{
	JsonWriter(OStream* stream=NULL);

	void setStream(OStream* stream);

	roStatus beginObject(const roUtf8* name=NULL);
	roStatus endObject();
	roStatus beginArray(const roUtf8* name=NULL);
	roStatus endArray();
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
	roStatus write(const roUtf8* name, const roByte* buf, roSize bufLen);

// Private
	roStatus flushStrBuf();
	OStream* _stream;
	String _strBuf;
	roSize _nestedLevel;
};  // JsonWriter

}   // namespace ro

#endif	// __roJson_h__
