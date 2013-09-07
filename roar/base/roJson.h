#ifndef __roJson_h__
#define __roJson_h__

#include "roString.h"
#include "roNonCopyable.h"

namespace ro {

struct OStream;

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

// Private:
	roStatus flushStrBuf();
	OStream* _stream;
	String _strBuf;
	roSize _nestedLevel;
};  // JsonWriter

}   // namespace ro

#endif	// __roJson_h__
