#include "pch.h"
#include "roIOStream.h"
#include "roTypeCast.h"
#include "roUtility.h"

namespace ro {

IStream::IStream()
{
	begin = current = end = NULL;
	_next = NULL;
	st = Status::ok;
}

Status IStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	if(bytesToRead == 0) {
		bytesRead = 0;
		return roStatus::ok;
	}

	if(current >= end)
		st = (*_next)(*this, bytesToRead);

	if(current >= end)
		return st;

	if(!buffer || !current)
		return roStatus::pointer_is_null;

	roAssert(current >= begin && current <= end);
	roSize remains = roSize(end - current);

	if(remains == 0)
		return Status::file_ended;

	roSize toRead = clamp_cast<roSize>(bytesToRead);
	toRead = roMinOf2(toRead, remains);
	roMemcpy(buffer, current, toRead);

	current += toRead;
	bytesRead = toRead;

	return roStatus::ok;
}

Status IStream::atomicRead(void* buffer, roUint64 bytesToRead)
{
	roByte* ptr = NULL;
	roUint64 size = bytesToRead;
	st = atomicPeek(ptr, size);
	if(!st) return st;

	st = read(buffer, bytesToRead, size);
	roAssert(size == bytesToRead);

	return st;
}

Status IStream::peek(roByte*& outBuf, roSize& outPeekSize)
{
	// Giving _next() a size of 0 to let it decide how much to put
	if(current >= end)
		st = (*_next)(*this, 1u);

	if(current >= end)
		return st;

	outBuf = current;
	outPeekSize = end - current;
	return st = Status::ok;
}

Status IStream::atomicPeek(roByte*& outBuf, roUint64& inoutMinBytesToPeek)
{
	while(st && (end - current) < inoutMinBytesToPeek)
		st = (*_next)(*this, inoutMinBytesToPeek);

	if((end - current) >= inoutMinBytesToPeek) {
		outBuf = current;
		inoutMinBytesToPeek = end - current;
		st = Status::ok;
	}

	return st;
}

Status IStream::skip(roUint64 bytes)
{
	if(end - current >= bytes) {
		current += bytes;
		return st = Status::ok;
	}

	return seekRead(bytes, SeekOrigin_Current);
}

static Status _next_eof(IStream& s, roUint64 bytesToRead)
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
	begin = buffer;
	current = buffer;
	end = begin + size;
}

Status MemoryIStream::size(roUint64& bytes) const
{
	bytes = end - begin;
	return roStatus::ok;
}

Status MemoryIStream::seekRead(roInt64 offset, SeekOrigin origin)
{
	roAssert(current >= begin && current <= end);
	roSize size = end - begin;
	if(origin == SeekOrigin_Begin) {
		if(offset < 0 || roIsGreater(offset, size)) return Status::file_seek_error;
		current = begin + offset;
	}
	else if(origin == SeekOrigin_Current) {
		roAssert(current >= begin);
		roSize currentOffset = current - begin;
		roSize remains = roAssertSub(size, currentOffset);
		if(roIsGreater(offset, remains)) return Status::file_seek_error;
		if(roIsGreater(-offset, currentOffset)) return Status::file_seek_error;
		current += offset;
	}
	else if(origin == SeekOrigin_End) {
		if(offset < 0 || roIsGreater(offset, size)) return Status::file_seek_error;
		current = begin + (size - offset);
	}

	roAssert(current >= begin && current <= end);

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
