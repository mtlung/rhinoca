#ifndef __roSerializer_h__
#define __roSerializer_h__

#include "roArray.h"
#include "roByteOrder.h"

#define roSERIALIZE_BYTE_ORDER (4321)	// The default endian to use in the serialization format. 4321 = little-endian (x86, ARM)

namespace ro {

struct Serializer
{
	Serializer();

	void	setBuf	(IByteArray* buf);
	Status	ioRaw	(roBytePtr p, roSize size);

	template<class T> Status io(T& value)		{ return serializeIo(*this, value); }
	template<class T> Status ioVary(T& value)	{ return serializeIoVary(*this, value); }

// Private
	Status	_checkRemain(roSize size);
	void	_advance	(roSize size);

	IByteArray* _buf;
	roBytePtr _w;
	roSize _used;
	roSize _remain;
};	// Serializer

struct Deserializer
{
	Deserializer();

	void	setBuf	(ByteArray* buf);
	Status	ioRaw	(roBytePtr p, roSize size);

	template<class T> Status io(T& value)		{ return serializeIo(*this, value); }
	template<class T> Status ioVary(T& value)	{ return serializeIoVary(*this, value); }

// Private
	Status	_checkRemain(roSize size);
	void	_advance	(roSize size);

	IByteArray* _buf;
	roBytePtr _r;
	roSize _used;
	roSize _remain;
};	// Deserializer


// ----------------------------------------------------------------------

inline Status Serializer::_checkRemain(roSize size)
{
	if(_remain >= size) return Status::ok;

	Status st = _buf->incSize(size - _remain); if(!st ) return st;

	_w = &(*_buf)[_used];
	_remain = size;

	return Status::ok;
}

inline void Serializer::_advance(roSize size) { _used += size; _remain -= size; }

inline Status Deserializer::_checkRemain(roSize size)
{
	if(_remain >= size) return Status::ok;
	return Status::end_of_data;
}

inline void Deserializer::_advance(roSize size) { _used += size; _remain -= size; _r += size; }


// ----------------------------------------------------------------------

#if roSERIALIZE_BYTE_ORDER == roBYTE_ORDER
#	define roHostToSe(x) (x)
#	define roSeToHost(x) (x)
#else
#	define roHostToSe(x) roByteSwap(x)
#	define roSeToHost(x) roByteSwap(x)
#endif

template<class T> inline Status serializePrimitive(Serializer& s, T& v)
{
	Status st = s._checkRemain(sizeof(T)); if(!st) return st;
	if(roCPU_SUPPORT_MEMORY_MISALIGNED >= sizeof(T))
		s._w.ref<T>() = roHostToSe(v);
	else {
		T tmp = roHostToSe(v);
		roBytePtr p = &tmp;
		roBytePtr w = s._w;
		for(roSize i=0; i<sizeof(T); ++i)
			*w++ = *p++;
	}
	s._advance(sizeof(T));
	return Status::ok;
}

template<class T> inline Status serializePrimitive(Deserializer& s, T& v)
{
	Status st = s._checkRemain(sizeof(T)); if(!st) return st;
	if(roCPU_SUPPORT_MEMORY_MISALIGNED >= sizeof(T))
		v = roSeToHost(s._r.ref<T>());
	else {
		T tmp;
		roBytePtr p = &tmp;
		roBytePtr r = s._r;
		for(roSize i=0; i<sizeof(T); ++i)
			*p++ = *r++;
		v = roSeToHost(tmp);
	}
	s._advance(sizeof(T));
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

Status serializeIoVary(Serializer& s, roUint16& v);
Status serializeIoVary(Serializer& s, roUint32& v);
Status serializeIoVary(Serializer& s, roUint64& v);
Status serializeIoVary(Deserializer& s, roUint16& v);
Status serializeIoVary(Deserializer& s, roUint32& v);
Status serializeIoVary(Deserializer& s, roUint64& v);


// ----------------------------------------------------------------------

inline Status serializeIo(Serializer& s, bool& v) {
	roUint8 tmp = v ? 1 : 0;
	return serializePrimitive(s, tmp);
}

inline Status serializeIo(Deserializer& s, bool& v) {
	roUint8 tmp;
	Status st = serializePrimitive(s, tmp); if(!st) return st;
	v = tmp > 0;
	return Status::ok;
}


// ----------------------------------------------------------------------

struct String;
extern Status serializeIo(Serializer& s, String& v);
extern Status serializeIo(Deserializer& s, String& v);

struct ConstString;
extern Status serializeIo(Serializer& s, ConstString& v);
extern Status serializeIo(Deserializer& s, ConstString& v);

}	// namespace ro

#endif	// __roSerializer_h__
