#ifndef __roSerializer_h__
#define __roSerializer_h__

#include "roArray.h"

namespace ro {

struct Serializer
{
	Serializer() : _buf(NULL), _used(0), _remain(0) {}

	void setBuf(ByteArray* buf) { _buf = buf; _used = 0; _remain = buf->size(); }

	Status ioRaw(roBytePtr p, roSize size)
	{
		Status st = checkRemain(size); if(!st) return st;
		roMemcpy(_p, p, size);
		_advance(size);
		return Status::ok;
	}

	template<class T> Status io(T& value) { return serializeIo(*this, value); }
	template<class T> Status ioVary(T& value) { return serializeIoVary(*this, value); }

	Status checkRemain(roSize size)
	{
		if(_remain >= size) return Status::ok;

		Status st = _buf->incSize(size - _remain); if(!st ) return st;

		_p = &(*_buf)[_used];
		_remain = size;

		return Status::ok;
	}

	void _advance(roSize size) { _used += size; _remain -= size; }

	ByteArray* _buf;
	roBytePtr _p;
	roSize _used;
	roSize _remain;
};	// Serializer

template<class T> inline Status serializePrimitive(Serializer& s, T& v)
{	
	Status st = s.checkRemain(sizeof(v)); if(!st) return st;
#if roCPU_SUPPORT_MEMORY_MISALIGNED >= 32
	*((T*)s._p) = roHostToLe(v);
#else
	roBytePtr p = &v;
	roBytePtr w = s._p;
	for(roSize i=0; i<sizeof(T); ++i)
		*w++ = *p++;
#endif
	s._advance(sizeof(v));
	return Status::ok;
}

template<class S> inline Status serializeIo(S& s, roInt8&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt16&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt32&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roInt64&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint8&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint16&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint32&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, roUint64&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, float&	v) { return serializePrimitive(s, v); }
template<class S> inline Status serializeIo(S& s, double&	v) { return serializePrimitive(s, v); }

template<class S> inline Status serializeIoVary(S& s, roUint32&	v)
{
	Status st;
	if(v < (1<<(7*1))) {
		st = s.checkRemain(1); if(!st) return st;
		s._p[0] = (roUint8) (v >> (7*0));
		s._advance(1);
		return st;
	}

	if(v < (1<<(7*2))) {
		st = s.checkRemain(2); if(!st) return st;
		s._p[0] = (roUint8) (v >> (7*0) | 0x80);
		s._p[1] = (roUint8) (v >> (7*1));
		s._advance(2);
		return st;
	}

	if(v < (1<<(7*3))) {
		st = s.checkRemain(3); if(!st) return st;
		s._p[0] = (roUint8) (v >> (7*0) | 0x80);
		s._p[1] = (roUint8) (v >> (7*1) | 0x80);
		s._p[2] = (roUint8) (v >> (7*2));
		s._advance(3);
		return st;
	}

	if(v < (1<<(7*4))) {
		st = s.checkRemain(4); if(!st) return st;
		s._p[0] = (roUint8) (v >> (7*0) | 0x80);
		s._p[1] = (roUint8) (v >> (7*1) | 0x80);
		s._p[2] = (roUint8) (v >> (7*2) | 0x80);
		s._p[3] = (roUint8) (v >> (7*3));
		s._advance(4);
		return st;
	}

	return Status::ok;
}


// ----------------------------------------------------------------------

struct String;

template<class S> inline Status serializeIo(S& s, String& v)
{
	roSize n = v.size();
	Status st;
	st = s.ioVary(n);			if(!st) return st;
	st = s.ioRaw(v.c_str(), n);	if(!st) return st;
	return 0;
}

}	// namespace ro

#endif	// __roSerializer_h__
