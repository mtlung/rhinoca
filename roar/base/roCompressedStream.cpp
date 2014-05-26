#include "pch.h"
#include "roCompressedStream.h"
#include "roTypeCast.h"

namespace ro {

// zlib reference:
// http://www.zlib.net/zlib_how.html

GZipStreamIStream::~GZipStreamIStream()
{
	if(_innerStream.ptr())
		inflateEnd(&_zStream);
}

Status GZipStreamIStream::init(AutoPtr<IStream>& stream, roSize chunkSize)
{
	if(_innerStream.ptr())
		inflateEnd(&_zStream);

	Status st;

	st = _inBuffer.resizeNoInit(chunkSize);
	_inBuffer.condense();
	if(!st) return st;

	roMemZeroStruct(_zStream);

	_zStream.zalloc = Z_NULL;
	_zStream.zfree = Z_NULL;
	_zStream.opaque = Z_NULL;
	_zStream.next_in = NULL;
	_zStream.avail_in = 0;
	_zStream.next_out = NULL;

	const unsigned windowBits = 15;
	const unsigned enableGzip = 32;
	int err = inflateInit2(&_zStream, windowBits | enableGzip);
	if(err != Z_OK) return Status::zlib_error;

	_innerStream.takeOver(stream);
	return Status::ok;
}

Status GZipStreamIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;
	if(bytesToRead == 0)
		return roStatus::ok;

	if(!_innerStream.ptr())
		return Status::pointer_is_null;

	while(true)	// Loop for read more input
	{
		if(_zStream.next_in == NULL || _zStream.avail_out != 0) {
			roUint64 len = 0;
			Status st = _innerStream->read(_inBuffer.bytePtr(), _inBuffer.sizeInByte(), len);
			if(!st) return st;

			_zStream.next_in = _inBuffer.bytePtr();
			st = roSafeAssign(_zStream.avail_in, len);
			if(!st) return st;
		}

		_zStream.next_out = (Bytef*)buffer;

		st = roSafeAssign(_zStream.avail_out, bytesToRead);
		if(!st) return st;

		int err = inflate(&_zStream, Z_NO_FLUSH);

		switch(err) {
		case Z_BUF_ERROR:	// Not enough input buffer, read more
			continue;
		case Z_STREAM_END:
			return Status::end_of_data;
		case Z_OK:
			break;
		default:
			return Status::zlib_error;
		}
		break;
	}

	bytesRead = bytesToRead - _zStream.avail_out;
	return st;
}

}   // namespace ro
