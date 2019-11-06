#include "pch.h"
#include "roCompressedStream.h"
#include "roTypeCast.h"

namespace ro {

// zlib reference:
// http://www.zlib.net/zlib_how.html

#if roCOMPILER_VC
#	pragma comment(lib, "zlib")
#endif

GZipIStream::~GZipIStream()
{
	if(_innerStream.ptr())
		inflateEnd(&_zStream);
}

Status GZipIStream::init(AutoPtr<IStream>&& stream, roSize chunkSize)
{
	if(_innerStream.ptr())
		inflateEnd(&_zStream);

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

	_innerStream = std::move(stream);
	return (st = Status::ok);
}

Status GZipIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;
	if(bytesToRead == 0)
		return (st = roStatus::ok);

	if(!buffer)
		return (st = roStatus::pointer_is_null);

	if(!_innerStream.ptr())
		return (st = Status::pointer_is_null);

	_zStream.next_out = (Bytef*)buffer;
	st = roSafeAssign(_zStream.avail_out, bytesToRead);
	if(!st) return st;

	while(true)	// Loop for read more input
	{
		if(_zStream.next_in == NULL || _zStream.avail_out != 0) {
			roUint64 len = 0;
			st = _innerStream->read(_inBuffer.bytePtr(), _inBuffer.sizeInByte(), len);
			if(!st) return st;

			_zStream.next_in = _inBuffer.bytePtr();
			st = roSafeAssign(_zStream.avail_in, len);
			if(!st) return st;
		}

		int err = inflate(&_zStream, Z_NO_FLUSH);

		switch(err) {
		case Z_OK:
		case Z_BUF_ERROR:	// Not enough input buffer, read more
			continue;
		case Z_STREAM_END:
			return (st = Status::ok);
		default:
			return (st = Status::zlib_error);
		}
	}

	bytesRead = bytesToRead - _zStream.avail_out;
	return st;
}

GZipOStream::~GZipOStream()
{
	flush();
	if(_innerStream.ptr())
		deflateEnd(&_zStream);
}

Status GZipOStream::init(AutoPtr<OStream>&& stream, roSize chunkSize)
{
	if(_innerStream.ptr())
		deflateEnd(&_zStream);

	roStatus st = _buffer.resizeNoInit(chunkSize);
	_buffer.condense();
	if(!st) return st;

	roMemZeroStruct(_zStream);

	_zStream.zalloc = Z_NULL;
	_zStream.zfree = Z_NULL;
	_zStream.opaque = Z_NULL;
	_zStream.next_in = NULL;
	_zStream.avail_in = 0;
	_zStream.next_out = _buffer.bytePtr();

	st = roSafeAssign(_zStream.avail_out, _buffer.sizeInByte());
	if(!st) return st;

	const unsigned windowBits = 15;
	const unsigned enableGzip = 16;
	int compressionLevel = 6;	// Z_DEFAULT_COMPRESSION
	int err = deflateInit2(&_zStream, compressionLevel, Z_DEFLATED, windowBits | enableGzip, 8, Z_DEFAULT_STRATEGY);
	if(err != Z_OK) return Status::zlib_error;

	_innerStream = std::move(stream);
	return (st = Status::ok);
}

Status GZipOStream::write(const void* buffer, roUint64 bytesToWrite)
{
	roStatus st = roStatus::ok;

	if(!_innerStream.ptr())
		return (st = Status::pointer_is_null);

	_zStream.next_in = (Bytef*)buffer;
	st = roSafeAssign(_zStream.avail_in, bytesToWrite);
	if(!st) return st;

	while(_zStream.avail_in)
	{
		int err = deflate(&_zStream, Z_NO_FLUSH);

		// Only writ to inner stream when zStream out buffer is full
		if(_zStream.avail_out == 0) {
			st = _innerStream->write(_buffer.bytePtr(), _buffer.sizeInByte() - _zStream.avail_out);
			if(!st) return st;
			_zStream.next_out = _buffer.bytePtr();
			st = roSafeAssign(_zStream.avail_out, _buffer.sizeInByte());
			if(!st) return st;
		}

		switch(err) {
		case Z_OK:
		case Z_BUF_ERROR:
		case Z_STREAM_END:
			continue;
		default:
			return (st = Status::zlib_error);
		}
	}

	return st;
}

roUint64 GZipOStream::posWrite() const
{
	return _zStream.total_in;
}

Status GZipOStream::flush()
{
	roStatus st = roStatus::ok;

	if(!_innerStream.ptr())
		return roStatus::pointer_is_null;

	while(true) {
		int err = deflate(&_zStream, Z_FINISH);
		switch (err) {
		case Z_OK:
		case Z_STREAM_END:
			st = _innerStream->write(_buffer.bytePtr(), _buffer.sizeInByte() - _zStream.avail_out);
			if(!st) return st;
			_zStream.next_out = _buffer.bytePtr();
			st = roSafeAssign(_zStream.avail_out, _buffer.sizeInByte());
			if(!st) return st;
			if(err == Z_STREAM_END)
				return _innerStream->flush();
		default:
			break;
		}
	}

	return roStatus::zlib_error;
}

Status GZipOStream::closeWrite()
{
	Status st = flush();
	if(!st) return st;

	return _innerStream->closeWrite();
}

}   // namespace ro
