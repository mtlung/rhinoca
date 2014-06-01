#include "pch.h"
#include "roHttp.h"
#include "../base/roIOStream.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roTypeCast.h"
#include "../base/roCompressedStream.h"

namespace ro {

static DefaultAllocator _allocator;

//////////////////////////////////////////////////////////////////////////
// HttpClient connection pool

roStatus HttpConnections::getConnection(const SockAddr& address, HttpConnection*& outConnection)
{
	outConnection = connections.find(address);
	if(outConnection)
		return roStatus::ok;

	AutoPtr<HttpConnection> newConn(_allocator.newObj<HttpConnection>());
	newConn->setKey(address);
	connections.insert(*newConn);
	newConn->socket.create(BsdSocket::TCP);

	roStatus st = newConn->socket.connect(address);
	if(!st) {
		outConnection = NULL;
		return st;
	}

	outConnection = newConn.unref();
	return roStatus::ok;
}

//////////////////////////////////////////////////////////////////////////
// HttpClient data streams

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
};	// HttpClientChunkedIStream

HttpClientChunkedIStream::HttpClientChunkedIStream(const CoSocket& socket)
	: _socket(const_cast<CoSocket&>(socket))
{
	_posInChunk = 0;
	_chunkSize = 0;
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
	if(val == 0)
		st = Status::end_of_data;

	return st;
}

Status HttpClientChunkedIStream::read(void* buffer, roUint64 bytesToRead, roUint64& bytesRead)
{
	bytesRead = 0;
	if(bytesToRead == 0)
		return roStatus::ok;

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
// HttpClient

HttpClient::HttpClient()
{
	logLevel = 0;
	useHttpCompression = true;
}

roStatus HttpClient::request(const HttpRequestHeader& orgHeader, ReplyFunc replyFunc, void* userPtr, HttpConnections* connectionPool)
{
	roStatus st;
	HttpRequestHeader header = orgHeader;

	HttpConnection* connection = NULL;
	HttpConnections dummyPool;

roEXCP_TRY
	if(!replyFunc)
		return roStatus::pointer_is_null;

	String host, resourcePath;

// Loop for location redirection
const roSize maxRedirect = 10;
roSize redirectCount = 0;
while(true) {
	String responseStr;
	roSize contentStartPos = 0;

	// Prepare request string
	{
		RangedString acceptEncoding;
		if(useHttpCompression && !header.getField(HttpRequestHeader::HeaderField::AcceptEncoding, acceptEncoding))
			header.addField(HttpRequestHeader::HeaderField::AcceptEncoding, "gzip");

		if(!header.getField(HttpRequestHeader::HeaderField::Host, host)) {
			st = roStatus::http_bad_header;
			roEXCP_THROW;
		}

		if(!header.getField(HttpRequestHeader::HeaderField::Resource, resourcePath)) {
			st = roStatus::http_bad_header;
			roEXCP_THROW;
		}
	}

	// Parse address string
	SockAddr addr;
	{	bool parseOk = false;
		if(host.find(':') != String::npos)
			parseOk = addr.parse(host.c_str());
		else
			parseOk = addr.parse(host.c_str(), 80);

		if(!parseOk) {
			roLog("error", "Fail to resolve host %s\n", host.c_str());
			st =  Status::net_resolve_host_fail;
			roEXCP_THROW;
		}
	}

	{	// Look for existing connection suitable for this request
		if(!connectionPool)
			connectionPool = &dummyPool;

		if(logLevel > 0)
			roLog("info", "HttpClient - Connecting to %s\n", host.c_str());

		st = connectionPool->getConnection(addr, connection);
		if(!st) roEXCP_THROW;

		roAssert(connection);
		connection->removeThis();
	}

	// Send request string
	{	String requestStr = header.string;
		requestStr += "\r\n";
		roSize len = requestStr.size();

		if(logLevel > 1)
			roLog("info", "HttpClient - Sending request string:\n%s", requestStr.c_str());
		else if(logLevel > 0)
			roLog("info", "HttpClient - Sending request string\n");

		st = connection->socket.send(requestStr.c_str(), len);
		if(!st) roEXCP_THROW;
	}

	// Read response
	HttpResponseHeader response;
	static const roSize maxResponseSize = 8 * 1024;
	{	static const char headerTerminator[] = "\r\n\r\n";
		roSize terminatorSize = sizeof(headerTerminator) - 1;
		roSize stepSize = 64;
		roSize searchIdx = 0;
		responseStr.reserve(stepSize);

		if(logLevel > 0)
			roLog("info", "HttpClient - Reading response\n");

		while(true) {
			roSize len = responseStr._capacity() - responseStr.size();
			if((responseStr.size() + len) > maxResponseSize) {
				st = roStatus::http_bad_header;
				roEXCP_THROW;
			}

			st = connection->socket.receive(responseStr.c_str() + responseStr.size(), len);
			if(!st) roEXCP_THROW;

			if(len == 0) {
				st = roStatus::net_connection_close;
				roEXCP_THROW;
			}

			st = responseStr.incSize(len);
			if(!st) roEXCP_THROW;

			roSize pos = responseStr.find(headerTerminator, searchIdx);
			if(pos == responseStr.npos) {
				searchIdx = responseStr.size();
				responseStr.reserve(responseStr.size() + stepSize);
				searchIdx = searchIdx < terminatorSize ? 0 : searchIdx - terminatorSize;
			}
			else {
				// Header terminator found
				response.string.assign(responseStr.c_str(), pos);
				contentStartPos = pos + terminatorSize;
				break;
			}
		}
	}

	if(logLevel > 1)
		roLog("info", "HttpClient - Response string:\n%s\n", response.string.c_str());

	// Get status code
	roUint64 statusCode = 0;
	if(!response.getField(HttpResponseHeader::HeaderField::Status, statusCode)) {
		st = roStatus::http_bad_header;
		roEXCP_THROW;
	}

	if(logLevel > 0)
		roLog("info", "HttpClient - Status code: %u\n", statusCode);

	// Decode status code
	switch(statusCode)
	{
	case 100:		// Continue
	case 404:
	case 200: {		// Ok
		AutoPtr<IStream> istream;

		roUint64 contentLength = 0;
		RangedString transferEncoding;

		// Determine the encoding
		if(response.getField(HttpResponseHeader::HeaderField::ContentLength, contentLength)) {
			AutoPtr<HttpClientSizedIStream> s = _allocator.newObj<HttpClientSizedIStream>(connection->socket, clamp_cast<roSize>(contentLength));
			s->begin = (roByte*)responseStr.c_str() + contentStartPos;
			s->end = (roByte*)responseStr.c_str() + responseStr.size();
			s->current = s->begin;
			istream.takeOver(s);

			if(logLevel > 0)
				roLog("info", "HttpClient - Content length: %u\n", contentLength);
		}
		else if(response.getField(HttpResponseHeader::HeaderField::TransferEncoding, transferEncoding)) {
			AutoPtr<HttpClientChunkedIStream> s = _allocator.newObj<HttpClientChunkedIStream>(connection->socket);
			s->begin = (roByte*)responseStr.c_str() + contentStartPos;
			s->end = (roByte*)responseStr.c_str() + responseStr.size();
			s->current = s->begin;
			istream.takeOver(s);
		}
		else {
			// Create a null stream
			AutoPtr<MemoryIStream> s = _allocator.newObj<MemoryIStream>();
			istream.takeOver(s);
		}

		// Any compression?
		RangedString contentEncoding;
		if(response.getField(HttpResponseHeader::HeaderField::ContentEncoding, contentEncoding)) {
			if(contentEncoding.cmpNoCase("gzip") == 0) {
				AutoPtr<GZipStreamIStream> s = _allocator.newObj<GZipStreamIStream>();
				s->init(istream);
				roAssert(!istream.ptr());
				istream.takeOver(s);

				if(logLevel > 0)
					roLog("info", "HttpClient - Content encoding: %s\n", contentEncoding.toString().c_str());
			}
		}

		if(logLevel > 0)
			roLog("info", "HttpClient - Read body content\n");

		st = (*replyFunc)(response, *istream, userPtr);
	}	break;
	case 206:		// Partial Content
		break;
	case 301:		// Moved Permanently
	case 302: {		// Found (http redirect)
		RangedString redirectLocation;
		if(!response.getField(HttpResponseHeader::HeaderField::Location, redirectLocation)) {
			st = roStatus::http_bad_header;
			roEXCP_THROW;
		}

		RangedString protocol, host_, resourcePath_;
		st = HttpClient::splitUrl(redirectLocation, protocol, host_, resourcePath_);
		if(st)
			host = host_, resourcePath = resourcePath_;
		else
			resourcePath = redirectLocation;

		if(resourcePath.isEmpty())
			resourcePath = "/";
		else if(resourcePath.front() != '/') {
			st = resourcePath.insert(0, "/");
			if(!st) roEXCP_THROW;
		}

		if(logLevel > 0)
			roLog("info", "HttpClient - Redirecting to: %s\n", redirectLocation.toString().c_str());

		header.make(HttpRequestHeader::Method::Get, resourcePath.c_str());
		header.addField(HttpRequestHeader::HeaderField::Host, host.c_str());

		++redirectCount;
		if(redirectCount > maxRedirect) {
			st = roStatus::http_max_redirect_reached;
			roEXCP_THROW;
		}

		// Release connection back to pool
		connectionPool->connections.insert(*connection);

		// Back to the start of the location redirect loop
		continue;
	}	break;
	case 400:		// Bad request
	case 401:		// Unauthorized
		break;
//	case 404:		// Not found
//		_nextFunc = &Connection::_funcReadBody;
		break;
	case 416:		// Requested Range not satisfiable
		break;
	default:
		break;
	}

	break;
}	// End of location redirection loop

roEXCP_CATCH
roEXCP_END
	// Release connection back to pool
	if(connection)
		connectionPool->connections.insert(*connection);

	return st;
}

roStatus HttpClient::splitUrl(const RangedString& url, RangedString& protocol, RangedString& host, RangedString& path)
{
	Regex regex;
	if(!regex.match(url, "^\\s*([A-Za-z]+)://([^/]+)([^\\r\\n]*)"))
		return Status::http_invalid_uri;

	protocol = regex.result[1];
	host = regex.result[2];
	path = regex.result[3];

	return Status::ok;
}

}	// namespace ro
