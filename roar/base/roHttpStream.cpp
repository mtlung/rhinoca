#include "pch.h"
#include "roIOStream.h"
#include "roLog.h"
#include "roArray.h"
#include "roRegex.h"
#include "roRingBuffer.h"
#include "roSocket.h"
#include "roStringFormat.h"
#include "roTypeCast.h"
#include "roCpuProfiler.h"

// Reference:
// http://royal.pingdom.com/2012/08/15/fun-and-unusual-http-response-headers/
namespace ro {

struct HttpIStream : public IStream
{
	HttpIStream();
	~HttpIStream();

			Status		open			(const roUtf8* path);
	virtual bool		readWillBlock	(roUint64 bytesToRead);
	virtual Status		size			(roUint64& bytes) const;
	virtual roUint64	posRead			() const;
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin);
	virtual void		closeRead		();

	Status		_connect			();
	Status		_waitConnect		();
	Status		_sendRequest		();
	Status		_readFromSocket		(roUint64 bytesToRead);
	Status		_readResponse		();
	Status		_readContent		(roUint64 bytesToRead);
	Status		_readChunkSize		(roUint64 bytesToRead);
	Status		_readChunkedContent	(roUint64 bytesToRead);
	Status		setError			();
	Status		setError			(Status s);

	bool		_debug;
	bool		_chunked;
	bool		_deflate;
	String		_host;
	String		_path;
	BsdSocket	_socket;
	SockAddr	_sockAddr;
	String		_requestStr;

	Status		_httpHeaderStatus;
	Status		_contentLengthSattus;
	roUint32	_responseCode;
	roUint64	_fileSize;
	RingBuffer	_ringBuf;
};	// HttpIStream

static Status _http_connect(IStream& s, roUint64 bytesToRead)			{ return static_cast<HttpIStream&>(s)._connect(); }
static Status _http_wait_connect(IStream& s, roUint64 bytesToRead)		{ return static_cast<HttpIStream&>(s)._waitConnect(); }
static Status _http_sendRequest(IStream& s, roUint64 bytesToRead)		{ return static_cast<HttpIStream&>(s)._sendRequest(); }
static Status _http_readResponse(IStream& s, roUint64 bytesToRead)		{ return static_cast<HttpIStream&>(s)._readResponse(); }
static Status _http_readContent(IStream& s, roUint64 bytesToRead)		{ return static_cast<HttpIStream&>(s)._readContent(bytesToRead); }
static Status _http_readChunkSize(IStream& s, roUint64 bytesToRead)		{ return static_cast<HttpIStream&>(s)._readChunkSize(bytesToRead); }
//static Status _http_readChunkedContent(IStream& s, roUint64 bytesToRead){ return static_cast<HttpIStream&>(s)._readChunkedContent(bytesToRead); }
static Status _http_error(IStream& s, roUint64 bytesToRead);

HttpIStream::HttpIStream()
{
	_debug = true;
	_next = _http_sendRequest;
	_httpHeaderStatus = Status::net_not_connected;

	roVerify(BsdSocket::initApplication() == 0);
}

HttpIStream::~HttpIStream()
{
	closeRead();
	roVerify(BsdSocket::closeApplication() == 0);
}

Status HttpIStream::setError()
{
	if(!st && st != Status::in_progress)
		_next = _http_error;
	return st;
}

Status HttpIStream::setError(Status s)
{
	if(!s && s != Status::in_progress)
		_next = _http_error;

	return st = s;
}

Status HttpIStream::open(const roUtf8* url)
{
	Regex regex;
	if(!regex.match(url, "^\\s*http://([^/]+)(.*)")) {
		roLog("error", "Invalid url: %s\n", url);
		return Status::http_invalid_uri;
	}

	_host = regex.result[1].toString();
	_path = regex.result[2].toString();

	_next = _http_connect;
	_fileSize = 0;

	return (*_next)(*this, 0);
}

bool HttpIStream::readWillBlock(roUint64 size)
{
	return false;
}

Status HttpIStream::_connect()
{
	if(_debug)
		roLog("debug", "HttpIStream: connect to %s\n", _host.c_str());

	roVerify(_socket.close() == 0);

	// Create socket
	roVerify(_socket.create(BsdSocket::TCP) == 0);
	_socket.setBlocking(false);

	_fileSize = 0;
	_ringBuf.clear();

	// Parse socket address
	if(_path.isEmpty())
		_path += "/";

	// NOTE: Currently this host resolving operation is blocking
	bool parseOk = false;
	if(_host.find(':') != String::npos)
		parseOk = _sockAddr.parse(_host.c_str());
	else
		parseOk = _sockAddr.parse(_host.c_str(), 80);

	if(!parseOk) {
		roLog("error", "Fail to resolve host %s\n", _host.c_str());
		return Status::net_resolve_host_fail;
	}

	// Make connection
	int ret = _socket.connect(_sockAddr);

	if(ret != 0 && !BsdSocket::inProgress(_socket.lastError)) {
		roLog("error", "Connection to %s failed\n", _host.c_str());
		return setError(Status::net_cannont_connect);
	}

	_next = _http_wait_connect;
	return (*_next)(*this, 0);
}

Status HttpIStream::_waitConnect()
{
	bool readable = false;
	bool writable = true;
	bool hasError = true;
	int ret = _socket.select(readable, writable, hasError);
	(void)ret;

	if(!writable && !hasError)
		return st = Status::in_progress;
	else if(!writable && hasError) {
		roLog("error", "Connection to %s failed\n", _host.c_str());
		return setError(Status::net_cannont_connect);
	}

	if(_debug) {
		roLog("debug", "HttpIStream: %s connected\n", _host.c_str());
		roLog("debug", "HttpIStream: GET %s\n", _path.c_str());
	}

	const char getFmt[] =
		"GET {} HTTP/1.1\r\n"
		"Host: {}\r\n"	// Required for http 1.1
		"User-Agent: The Roar Engine\r\n"
		"Range: bytes=0-64\r\n"
		"\r\n";

	_requestStr.clear();
	strFormat(_requestStr, getFmt, _path.c_str(), _host.c_str());

	_next = _http_sendRequest;
	return (*_next)(*this, 0);
}

Status HttpIStream::_sendRequest()
{
	roAssert(!_requestStr.isEmpty());

	int ret = _socket.send(_requestStr.c_str(), _requestStr.size());
	if(ret < 0 && !BsdSocket::inProgress(_socket.lastError))
		return setError(Status::net_error);

	_next = _http_readResponse;
	return st;
}

Status HttpIStream::_readFromSocket(roUint64 bytesToRead)
{
	roSize byteSize = 0;
	st = roSafeAssign(byteSize, bytesToRead);
	if(!st) return setError();

	roByte* wPtr = NULL;
	st = _ringBuf.write(byteSize, wPtr);
	if(!st) return setError();

	int ret = _socket.receive(wPtr, byteSize);
	_ringBuf.commitWrite(ret > 0 ? ret : 0);

	if(ret < 0 && BsdSocket::inProgress(_socket.lastError))
		return setError(Status::in_progress);
	if(ret < 0)
		return setError(Status::net_error);
	if(ret == 0)
		return setError(Status::file_ended);

	return st;
}

struct HttpResponseHeader
{
	Status		parse		(char* buf, roSize bufSize, roSize& headerSize);
	const char*	findValue	(const char* name);

	struct Pair {
		char* name;
		char* value;
	};
	TinyArray<Pair, 16> nvp;
};

Status HttpResponseHeader::parse(char* buf, roSize bufSize, roSize& headerSize)
{
	// Check if header complete
	static const char headerTerminator[] = "\r\n\r\n";

	const char* messageContent = roStrnStr(buf, bufSize, headerTerminator);
	if(!messageContent)	// We need to read more till we see the header terminator
		return Status::in_progress;

	// Skip the header terminator
	messageContent += 4;
	headerSize = messageContent - buf;

	nvp.clear();

	// Tokenize the string by new line
	for(char* begin = buf, *end = NULL; end + 2 < messageContent; )
	{
		end = roStrStr(buf, messageContent, "\r\n");
		*end = '\0';

		Pair pair = { NULL };

		// Split by ':'
		pair.name = begin;
		char* p = roStrStr(begin, end, ":");
		if(p) {
			*p = '\0';
			++p;
		}
		pair.value = p;
		Status st = nvp.pushBack(pair);
		if(!st) return st;

		begin = end += 2;
	}

	if(nvp.isEmpty())
		return Status::http_error;

	return Status::ok;
}

const char*	HttpResponseHeader::findValue(const char* name)
{
	for(roSize i=0; i<nvp.size(); ++i) {
		if(roStrCaseCmp(nvp[i].name, name) == 0)
			return nvp[i].value;
	}
	return NULL;
}

Status HttpIStream::_readResponse()
{
	st = _readFromSocket(1024);
	if(!st) return setError();

	roSize readSize = 0;
	roByte* rPtr = _ringBuf.read(readSize);

	roSize headerSize = 0;
	HttpResponseHeader responseHeader;
	st = responseHeader.parse((char*)rPtr, readSize, headerSize);
	if(!st) return setError();

	{	// Parse response
		Regex regex;
		if(!regex.match(responseHeader.nvp[0].name, "^HTTP/([\\d\\.]+)[ ]+(\\d+)")) {
			roLog("error", "Invalid http response header: %s\n", responseHeader.nvp[0].name);
			return setError(Status::http_error);
		}

		regex.result[2].end = '\0';
		_responseCode = roStrToUint32(regex.result[2].begin, 0);

		if(_responseCode == 200)			// Ok
		{
			_chunked = false;
			_deflate = false;

			// Reference: http://www.iana.org/assignments/http-parameters/http-parameters.xhtml
			const char* encoding = responseHeader.findValue("Transfer-Encoding");
			if(encoding) {
				_chunked = roStrStrCase((char*)encoding, "chunked");
				_deflate = roStrStrCase((char*)encoding, "deflate");
			}

			const char* contentLength = responseHeader.findValue("Content-Length");
			if(contentLength) {
				if(sscanf(contentLength, "%*[ \t]%u", &_fileSize) != 1)
					st = _contentLengthSattus = Status::http_error;
				else
					_contentLengthSattus = Status::ok;
			}
			else
				_contentLengthSattus = Status::not_supported;

			if(_chunked)
				_next = _http_readChunkSize;
			else
				_next = _http_readContent;
		}
		else if(_responseCode == 206)		// Partial Content
		{
			_responseCode = 206;
		}
		else if(
			_responseCode == 301 ||			// Moved Permanently
			_responseCode == 302			// Found (http redirect)
		)
		{
			_next = _http_connect;
			st = Status::in_progress;

			const char* location = responseHeader.findValue("Location");
			if(!location)
				return setError(Status::http_error);

			return open(location);
		}
		else
		{
			st = Status::http_error;
			roLog("error", "Http stream receive server error code '%d'\n", _responseCode);
		}
	}

	_ringBuf.commitRead(headerSize);

	_httpHeaderStatus = st;
	if(!st) return setError();
	return st;
}

Status HttpIStream::_readContent(roUint64 bytesToRead)
{
	st = _readFromSocket(bytesToRead);

	roSize readableSize = 0;
	st = roSafeAssign(readableSize, bytesToRead);
	if(!st) return st;

	begin = _ringBuf.read(readableSize);
	current = begin;
	end = begin + readableSize;

	return st;
}

Status HttpIStream::_readChunkSize(roUint64 bytesToRead)
{
	return st;
}

Status HttpIStream::_readChunkedContent(roUint64 bytesToRead)
{
	st = _readFromSocket(bytesToRead);

	roSize readableSize = 0;
	st = roSafeAssign(readableSize, bytesToRead);
	if(!st) return st;

	begin = _ringBuf.read(readableSize);
	current = begin;
	end = begin + readableSize;

	return st;
}

static Status _http_error(IStream& s, roUint64 bytesToRead)
{
	return Status::http_error;
}

Status HttpIStream::size(roUint64& bytes) const
{
	HttpIStream& self = const_cast<HttpIStream&>(*this);
	if(!_httpHeaderStatus)
		self._httpHeaderStatus = (*_next)(self, 0);

	if(!_httpHeaderStatus)
		return _httpHeaderStatus;

	if(!_contentLengthSattus)
		return _contentLengthSattus;

	bytes = self._fileSize;
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
	_socket.close();
	_ringBuf.clear();
}

Status openHttpIStream(roUtf8* url, AutoPtr<IStream>& stream)
{
	AutoPtr<HttpIStream> s = defaultAllocator.newObj<HttpIStream>();
	if(!s.ptr()) return Status::not_enough_memory;

	Status st = s->open(url);
	if(!st && st != Status::in_progress)
		return st;

	stream.takeOver(s);
	return st;
}

}	// namespace ro
