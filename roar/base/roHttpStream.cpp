#include "pch.h"
#include "roIOStream.h"
#include "roLog.h"
#include "roArray.h"
#include "roRegex.h"
#include "roRingBuffer.h"
#include "roSocket.h"
#include "roTypeCast.h"
#include "roCpuProfiler.h"

namespace ro {

struct HttpIStream : public IStream
{
	HttpIStream();

			Status		open			(const roUtf8* path);
	virtual bool		readWillBlock	(roUint64 bytesToRead);
	virtual Status		size			(roUint64& bytes) const;
	virtual roUint64	posRead			() const;
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin);
	virtual void		closeRead		();

	BsdSocket	_socket;
	roUint64	_fileSize;
	RingBuffer	_ringBuf;
};	// HttpIStream

static Status _http_sendRequest(IStream& s, roUint64 bytesToRead);

HttpIStream::HttpIStream()
{
	_next = _http_sendRequest;
}

Status HttpIStream::open(const roUtf8* url)
{
	Regex regex;
	regex.match(url, "^http://([^/]+)(.*)");

	const RangedString& host = regex.result[1];
	const RangedString& path = regex.result[2];

	SockAddr addr;

	return st = Status::ok;
}

bool HttpIStream::readWillBlock(roUint64 size)
{
	return false;
}

static Status _http_sendRequest(IStream& s, roUint64 bytesToRead)
{
	HttpIStream& self = static_cast<HttpIStream&>(s);

	return self.st;
}

Status HttpIStream::size(roUint64& bytes) const
{
	return st = Status::ok;
}

roUint64 HttpIStream::posRead() const
{
	return 0;
}

Status HttpIStream::seekRead(roInt64 offset, SeekOrigin origin)
{
	return st = Status::ok;
}

void HttpIStream::closeRead()
{
}

Status openHttpIStream(roUtf8* url, AutoPtr<IStream>& stream)
{
	AutoPtr<HttpIStream> s = defaultAllocator.newObj<HttpIStream>();
	if(!s.ptr()) return Status::not_enough_memory;

	Status st = s->open(url);
	if(!st) return st;

	stream.takeOver(s);
	return st;
}

}	// namespace ro
