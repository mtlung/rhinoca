#include "pch.h"
#include "roSerializer.h"
#include "roString.h"

namespace ro {

Serializer::Serializer()
	: _buf(NULL), _used(0), _remain(0)
{}

void Serializer::setBuf(IByteArray* buf)
{
	_buf = buf;
	_w = NULL;
	_used = 0;
	_remain = buf->size();
}

Status Serializer::ioRaw(roBytePtr p, roSize size)
{
	Status st = _checkRemain(size); if(!st) return st;
	roMemcpy(_w, p, size);
	_advance(size);
	return Status::ok;
}

Deserializer::Deserializer()
	: _buf(NULL), _used(0), _remain(0)
{}

void Deserializer::setBuf(ByteArray* buf)
{
	_buf = buf;
	_r = &(*buf)[0];
	_used = 0;
	_remain = buf->size();
}

Status Deserializer::ioRaw(roBytePtr p, roSize size)
{
	if(_remain < size) return Status::end_of_data;
	roMemcpy(p, _r, size);
	_advance(size);
	return Status::ok;
}

Status serializeIoVary(Serializer& s, roUint32& v_)
{
	Status st;
	roUint32 v = v_;

	if(v < (1<<(7*1))) {
		st = s._checkRemain(1); if(!st) return st;
		s._w[0] = (roUint8) (v >> (7*0));
		s._advance(1);
		return st;
	}

	if(v < (1<<(7*2))) {
		st = s._checkRemain(2); if(!st) return st;
		s._w[0] = (roUint8) (v >> (7*0) | 0x80);
		s._w[1] = (roUint8) (v >> (7*1));
		s._advance(2);
		return st;
	}

	if(v < (1<<(7*3))) {
		st = s._checkRemain(3); if(!st) return st;
		s._w[0] = (roUint8) (v >> (7*0) | 0x80);
		s._w[1] = (roUint8) (v >> (7*1) | 0x80);
		s._w[2] = (roUint8) (v >> (7*2));
		s._advance(3);
		return st;
	}

	if(v < (1<<(7*4))) {
		st = s._checkRemain(4); if(!st) return st;
		s._w[0] = (roUint8) (v >> (7*0) | 0x80);
		s._w[1] = (roUint8) (v >> (7*1) | 0x80);
		s._w[2] = (roUint8) (v >> (7*2) | 0x80);
		s._w[3] = (roUint8) (v >> (7*3));
		s._advance(4);
		return st;
	}

	else {
		st = s._checkRemain(5); if(!st) return st;
		s._w[0] = (roUint8) (v >> (7*0) | 0x80);
		s._w[1] = (roUint8) (v >> (7*1) | 0x80);
		s._w[2] = (roUint8) (v >> (7*2) | 0x80);
		s._w[3] = (roUint8) (v >> (7*3) | 0x80);
		s._w[4] = (roUint8) (v >> (7*4));
		s._advance(5);
		return st;
	}

	return Status::assertion;
}

Status serializeIoVary(Serializer& s, roUint64& v_)
{
	// Unlike the 32-bit version, we use a loop here to save code size
	Status st;

	roUint64 v = v_;
	roSize i;
	for(i=0; i<roSize((sizeof(v) * 8)/7); ++i) {
		if(v >= (1ULL<<(7*(i+1)))) {
			st = s._checkRemain(1); if(!st) return st;
			s._w[0] = (roUint8) (v >> (7*i) | 0x80);
			s._advance(1);
		}
		else
			break;
	}

	st = s._checkRemain(1); if(!st) return st;
	s._w[0] = (roUint8) (v >> (7*i));
	s._advance(1);

	return st;
}

Status serializeIoVary(Deserializer& s, roUint32& v)
{
	Status st;
	roSize bit = 0;
	roUint8 t;
	v = 0;
	for(;;)
	{
		st = s._checkRemain(1); if(!st) return st;
		t = *s._r;
		v |= (roUint32)(t & 0x7F) << bit;
		s._advance(1);

		if(!(t & 0x80))
			return Status::ok;

		bit += 7;

		if(bit > sizeof(v)*8) {
			roAssert(false);
			return Status::assertion;
		}
	}

	return Status::assertion;
}

Status serializeIoVary(Deserializer& s, roUint64& v)
{
	Status st;
	roSize bit = 0;
	roUint8 t;
	v = 0;
	for(;;)
	{
		st = s._checkRemain(1); if(!st) return st;
		t = *s._r;
		v |= (roUint64)(t & 0x7F) << bit;
		s._advance(1);

		if(!(t & 0x80))
			return Status::ok;

		bit += 7;

		if(bit > sizeof(v)*8) {
			roAssert(false);
			return Status::assertion;
		}
	}

	return Status::assertion;
}


// ----------------------------------------------------------------------

Status serializeIo(Serializer& s, String& v)
{
	roSize n = v.size();
	Status st;
	st = s.ioVary(n);			if(!st) return st;
	st = s.ioRaw(v.c_str(), n);	if(!st) return st;
	return Status::ok;
}

Status serializeIo(Deserializer& s, String& v)
{
	Status st;
	roSize n;
	st = s.ioVary(n);			if(!st) return st;
	st = v.resize(n);			if(!st) return st;
	st = s.ioRaw(v.c_str(), n);	if(!st) return st;
	return Status::ok;
}

Status serializeIo(Serializer& s, ConstString& v)
{
	roSize n = v.size();
	Status st;
	st = s.ioVary(n);			if(!st) return st;
	st = s.ioRaw(v.c_str(), n);	if(!st) return st;
	return Status::ok;
}

Status serializeIo(Deserializer& s, ConstString& v)
{
	String tmp;
	Status st = serializeIo(s, tmp); if(!st) return st;
	v = tmp.c_str();
	return Status::ok;
}

}	// namespace ro
