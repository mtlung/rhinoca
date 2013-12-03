#include "pch.h"
#include "roIOStream.h"
#include "roTypeCast.h"
#include "roUtility.h"

namespace ro {

IStream::IStream()
{
	_begin = _current = _end = NULL;
	_next = NULL;
}

Status IStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	if(bytesToRead == 0)
		return roStatus::ok;

	if(!buffer || !_current)
		return roStatus::pointer_is_null;

	if(_current == _end) {
		if(!_next) return roStatus::pointer_is_null;
		Status st = (*_next)(*this);
		if(!st) return st;
	}

	roAssert(_current >= _begin && _current <= _end);
	roSize remains = roSize(_end - _current);

	if(remains == 0)
		return Status::file_ended;

	roSize toRead = clamp_cast<roSize>(bytesToRead);
	toRead = roMinOf2(toRead, remains);
	roMemcpy(buffer, _current, toRead);

	_current += toRead;
	bytesRead = toRead;

	return roStatus::ok;
}

Status IStream::atomicRead(void* buffer, roUint64 bytesToRead)
{
	roUint64 bytesRead = 0;
	Status st = read(buffer, bytesToRead, bytesRead);
	if(!st) return st;
	if(bytesRead < bytesToRead) return Status::file_ended;
	return st;
}

static Status _next_eof(IStream& s)
{
	return Status::end_of_data;
}

MemoryIStream::MemoryIStream(roByte* buffer, roSize size)
{
	_next = _next_eof;
	reset(buffer, size);
}

void MemoryIStream::reset(roByte* buffer, roSize size)
{
	_begin = buffer;
	_current = buffer;
	_end = _begin + size;
}

Status MemoryIStream::size(roUint64& bytes) const
{
	bytes = _end - _begin;
	return roStatus::ok;
}

Status MemoryIStream::seekRead(roInt64 offset, SeekOrigin origin)
{
	roAssert(_current >= _begin && _current <= _end);
	roSize size = _end - _begin;
	if(origin == SeekOrigin_Begin) {
		if(offset < 0 || roIsGreater(offset, size)) return Status::file_seek_error;
		_current = _begin + offset;
	}
	else if(origin == SeekOrigin_Current) {
		roAssert(_current >= _begin);
		roSize currentOffset = _current - _begin;
		roSize remains = roAssertSub(size, currentOffset);
		if(roIsGreater(offset, remains)) return Status::file_seek_error;
		if(roIsGreater(-offset, currentOffset)) return Status::file_seek_error;
		_current += offset;
	}
	else if(origin == SeekOrigin_End) {
		if(offset < 0 || roIsGreater(offset, size)) return Status::file_seek_error;
		_current = _begin + (size - offset);
	}

	roAssert(_current >= _begin && _current <= _end);

	return roStatus::ok;
}

Status MemoryOStream::write(const void* buffer, roUint64 bytesToWrite)
{
	return _buf.insert(_buf.size(), (roUint8*)buffer, (roUint8*)buffer + bytesToWrite);
}

MemorySeekableOStream::MemorySeekableOStream()
	: _pos(0)
{
}

Status MemorySeekableOStream::write(const void* buffer, roUint64 bytesToWrite)
{
	roSize bytesToWrite_;
	roStatus st = roSafeAssign(bytesToWrite_, bytesToWrite);
	if(!st) return st;

	roAssert(size() >= _pos);
	if(bytesToWrite_ > (size() - _pos)) {
		roUint64 newSize;
		roStatus st = roSafeAdd(_pos, bytesToWrite_, newSize);
		if(!st) return st;
		st = roIsValidCast(roSize(0), newSize);
		if(!st) return st;
		st = _buf.resizeNoInit(num_cast<roSize>(newSize));
		if(!st) return st;
	}

	roMemcpy(_buf.castedPtr<roByte>() + _pos, buffer, bytesToWrite_);
	_pos += bytesToWrite_;

	return roStatus::ok;
}

Status MemorySeekableOStream::seekWrite(roInt64 offset, SeekOrigin origin)
{
	if(origin == SeekOrigin_Begin) {
		if(offset < 0 || roIsGreater(offset, size())) return Status::file_seek_error;
		return roSafeAssign(_pos, offset);
	}
	else if(origin == SeekOrigin_Current) {
		roSize remains = roAssertSub(size(), num_cast<roSize>(_pos));
		if(roIsGreater(offset, remains)) return Status::file_seek_error;
		if(roIsGreater(-offset, _pos)) return Status::file_seek_error;
		_pos += SignedCounterPart<roSize>::Ret(offset);
	}
	else if(origin == SeekOrigin_End) {
		if(offset < 0 || roIsGreater(offset, size())) return Status::file_seek_error;
		_pos = size() - SignedCounterPart<roSize>::Ret(offset);
	}

	return roStatus::ok;
}


}   // namespace ro
