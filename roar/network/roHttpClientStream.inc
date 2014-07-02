//////////////////////////////////////////////////////////////////////////
// HttpClient data streams

namespace ro {

//////////////////////////////////////////////////////////////////////////
// HttpClientSizedIStream

struct HttpClientSizedIStream : public IStream
{
	HttpClientSizedIStream(const CoSocket& socket, roSize size);

	virtual Status size(roUint64& bytes) const override;
	virtual	Status read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead) override;

	roSize _size;
	roSize _readPos;
	CoSocket& _socket;
};	// HttpClientSizedIStream

HttpClientSizedIStream::HttpClientSizedIStream(const CoSocket& socket, roSize size)
	: _socket(const_cast<CoSocket&>(socket))
{
	_size = size;
	_readPos = 0;
}

Status HttpClientSizedIStream::size(roUint64& bytes) const
{
	bytes = _size;
	return roStatus::ok;
}

Status HttpClientSizedIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;

	if(bytesToRead == 0)
		return roStatus::ok;

	if(_readPos >= _size)
		return Status::end_of_data;

	if(current < end) {
		roUint64 available = end - current;
		bytesRead = roMinOf2(bytesToRead, available);
		roMemcpy(buffer, current, clamp_cast<roSize>(bytesRead));
		current += bytesRead;
	}
	else {
		roSize len = clamp_cast<roSize>(bytesToRead);
		st = _socket.receive(buffer, len);
		bytesRead = len;
	}

	_readPos += clamp_cast<roSize>(bytesRead);

	roAssert(current >= begin && current <= end);
	return roStatus::ok;
}


//////////////////////////////////////////////////////////////////////////
// HttpClientChunkedIStream

struct HttpClientChunkedIStream : public IStream
{
	HttpClientChunkedIStream(const CoSocket& socket);

	virtual	Status read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead) override;

	Status _readChunkSize();
	Status _readFormBuf(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
	Status _readFormSocket(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
	Status (HttpClientChunkedIStream::*_innerRead)(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);

	roUint64 _posInChunk;
	roUint64 _chunkSize;
	CoSocket& _socket;
	bool _ended;
};	// HttpClientChunkedIStream

HttpClientChunkedIStream::HttpClientChunkedIStream(const CoSocket& socket)
	: _socket(const_cast<CoSocket&>(socket))
{
	_posInChunk = 0;
	_chunkSize = 0;
	_ended = false;
	_innerRead = &HttpClientChunkedIStream::_readFormBuf;
}

Status HttpClientChunkedIStream::_readFormBuf(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	if(bytesToRead == 0) {
		bytesRead = 0;
		return roStatus::ok;
	}

	if(current < end) {
		roUint64 available = end - current;
		bytesRead = roMinOf2(bytesToRead, available);
		roMemcpy(buffer, current, clamp_cast<roSize>(bytesRead));
		current += bytesRead;
	}
	else {
		_innerRead = &HttpClientChunkedIStream::_readFormSocket;
		return _readFormSocket(buffer, bytesToRead, bytesRead);
	}

	roAssert(current >= begin && current <= end);
	return roStatus::ok;
}

Status HttpClientChunkedIStream::_readFormSocket(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	roSize size = clamp_cast<roSize>(bytesToRead);
	Status st = _socket.receive(buffer, size);
	bytesRead = size;
	return st;
}

Status HttpClientChunkedIStream::_readChunkSize()
{
	Status st;
	const roSize maxHexStrSize = 16;
	char buf[maxHexStrSize + 3];	// 16 for the hex string, two for "\r\n" and one for safety null
	buf[sizeof(buf) - 1] = '\0';
	char* p = buf;
	roUint64 len = 0;
	roUint64 val = 0;

	while(p - buf < maxHexStrSize) {
		st = (this->*_innerRead)(p, 1, len);
		if(!st) return st;
		if(len == 0) return Status::http_invalid_chunk_size;
		roAssert(len == 1);
		++p;
		const char* newPos = p;
		st = roHexStrTo(buf, newPos, val);
		if(!st) return st;

		if(newPos == p - 2) {
			p -= 2;
			break;
		}
	}

	if(p[0] != '\r' && p[1] != '\n')
		return Status::http_invalid_chunk_size;

	_chunkSize = val;
	if(val == 0) {
		_ended = true;

		// Real till another \r\n:
		// 0
		// some-footer: some-value
		// another-footer: another-value
		// [blank line here]
		while(st) {
			char c;
			st = (this->*_innerRead)(&c, 1, len);
			if(len == 0) return Status::http_invalid_chunk_size;

			if(c == '\r') {
				st = (this->*_innerRead)(&c, 1, len);
				if(len == 0) return Status::http_invalid_chunk_size;
				if(c == '\n')
					st = Status::end_of_data;
			}
		}
	}

	return st;
}

Status HttpClientChunkedIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;
	if(bytesToRead == 0)
		return roStatus::ok;

	if(_ended)
		return Status::end_of_data;

	// Try to parse the chunk size
	if(_posInChunk == _chunkSize) {
		st = _readChunkSize();

		_posInChunk = 0;

		if(!st) return st;
	}

	bytesToRead = roMinOf2(bytesToRead, _chunkSize - _posInChunk);
	st = (this->*_innerRead)(buffer, bytesToRead, bytesRead);
	_posInChunk += bytesRead;

	if(!st) return st;

	// Consume th e"\r\n" after each chunk (even for chunk with 0 size)
	if(_posInChunk == _chunkSize) {
		char buf[2];
		roUint64 len = 0;
		st = (this->*_innerRead)(buf, 2, len);
		if(st && len != 2)
			st = Status::http_invalid_chunk_size;
	}

	roAssert(current >= begin && current <= end);
	return st;
}


//////////////////////////////////////////////////////////////////////////
// HttpClientReadTillEndIStream

struct HttpClientReadTillEndIStream : public IStream
{
	HttpClientReadTillEndIStream(const CoSocket& socket);

	virtual	Status read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead) override;

	Status _readChunkSize();
	Status _readFormBuf(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
	Status _readFormSocket(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
	Status (HttpClientReadTillEndIStream::*_innerRead)(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);

	CoSocket& _socket;
};	// HttpClientChunkedIStream

HttpClientReadTillEndIStream::HttpClientReadTillEndIStream(const CoSocket& socket)
	: _socket(const_cast<CoSocket&>(socket))
{
}

Status HttpClientReadTillEndIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;

	if(bytesToRead == 0)
		return roStatus::ok;

	if(current < end) {
		roUint64 available = end - current;
		bytesRead = roMinOf2(bytesToRead, available);
		roMemcpy(buffer, current, clamp_cast<roSize>(bytesRead));
		current += bytesRead;
	}
	else {
		roSize len = clamp_cast<roSize>(bytesToRead);
		st = _socket.receive(buffer, len);
		bytesRead = len;
	}

	roAssert(current >= begin && current <= end);
	return bytesRead ? roStatus::ok : Status::end_of_data;
}

}	// namespace ro
