#ifndef __roSerializer_h__
#define __roSerializer_h__

#include "roArray.h"

namespace ro {

struct Serializer
{
	Serializer();

	void	setBuf	(ByteArray* buf);
	Status	ioRaw	(roBytePtr p, roSize size);

	template<class T> Status io(T& value) { return serializeIo(*this, value); }
	template<class T> Status ioVary(T& value) { return serializeIoVary(*this, value); }

	Status checkRemain(roSize size)
	{
		if(_remain >= size) return Status::ok;

		Status st = _buf->incSize(size - _remain); if(!st ) return st;

		_w = &(*_buf)[_used];
		_remain = size;

		return Status::ok;
	}

	void _advance(roSize size) { _used += size; _remain -= size; }

	ByteArray* _buf;
	roBytePtr _w;
	roSize _used;
	roSize _remain;
};	// Serializer

struct Deserializer
{
	Deserializer();

	void	setBuf	(ByteArray* buf);
	Status	ioRaw	(roBytePtr p, roSize size);

	template<class T> Status io(T& value) { return serializeIo(*this, value); }
	template<class T> Status ioVary(T& value) { return serializeIoVary(*this, value); }

	Status checkRemain(roSize size)
	{
		if(_remain >= size) return Status::ok;
		return Status::end_of_data;
	}

	void _advance(roSize size) { _used += size; _remain -= size; _r += size; }

	ByteArray* _buf;
	roBytePtr _r;
	roSize _used;
	roSize _remain;
};	// Deserializer

template<class T> inline Status serializePrimitive(Serializer& s, T& v)
{
	Status st = s.checkRemain(sizeof(v)); if(!st) return st;
#if roCPU_SUPPORT_MEMORY_MISALIGNED >= 32
	*((T*)s._w) = roHostToLe(v);
#else
	roBytePtr p = &v;
	roBytePtr w = s._w;
	for(roSize i=0; i<sizeof(T); ++i)
		*w++ = *p++;
#endif
	s._advance(sizeof(v));
	return Status::ok;
}

template<class T> inline Status serializePrimitive(Deserializer& s, T& v)
{
	Status st = s.checkRemain(sizeof(v)); if(!st) return st;
#if roCPU_SUPPORT_MEMORY_MISALIGNED >= 32
	v = roLeToHost(*((T*)s._r));
#else
	T tmp;
	roBytePtr p = &tmp;
	roBytePtr r = s._r;
	for(roSize i=0; i<sizeof(T); ++i)
		*p++ = *r++;
//	v = roLeToHost(tmp);
	v = tmp;
#endif
	s._advance(sizeof(v));
	return Status::ok;
}

template<class S> inline Status serializeIo(S& s, roInt8&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt16&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt32&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt64&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint8&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint16&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint32&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint64&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, float&		v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, double&		v) { return serializePrimitive(s, v); }

Status serializeIoVary(Serializer& s, roUint32& v);
Status serializeIoVary(Deserializer& s, roUint32& v);


// ----------------------------------------------------------------------
inline Status serializeIo(Serializer& s, bool& v) {
	roUint8 tmp = v ? 1 : 0;
	return serializePrimitive(s, tmp);
}

inline Status serializeIo(Deserializer& s, bool& v) {
	roUint8 tmp;
	Status st = serializePrimitive(s, tmp); if(!st) return st;
	v = tmp;
	return Status::ok;
}

// ----------------------------------------------------------------------
struct String;
extern Status serializeIo(Serializer& s, String& v);
extern Status serializeIo(Deserializer& s, String& v);

}	// namespace ro

#endif	// __roSerializer_h__
