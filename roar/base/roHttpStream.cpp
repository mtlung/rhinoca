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

			Status		setError		();
			Status		setError		(Status s);

	String		_host;
	String		_path;
	BsdSocket	_socket;
	SockAddr	_sockAddr;
	String		_requestStr;

	Status		_httpHeaderStatus;
	roUint64	_fileSize;
	RingBuffer	_ringBuf;
};	// HttpIStream

static Status _http_connect(IStream& s, roUint64 bytesToRead);
static Status _http_sendRequest(IStream& s, roUint64 bytesToRead);
static Status _http_readResponse(IStream& s, roUint64 bytesToRead);
static Status _http_error(IStream& s, roUint64 bytesToRead);

HttpIStream::HttpIStream()
{
	_next = _http_sendRequest;
	_httpHeaderStatus = Status::net_not_connected;
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
	if(!regex.match(url, "^http://([^/]+)(.*)")) {
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

static Status _http_connect(IStream& s, roUint64 bytesToRead)
{
	HttpIStream& self = static_cast<HttpIStream&>(s);
	BsdSocket& socket = self._socket;
	String& host = self._host;

	self._fileSize = 0;
	self._ringBuf.clear();

	// Parse socket address
	if(self._path.isEmpty())
		self._path += "/";

	// NOTE: Currently this host resolving operation is blocking
	bool parseOk = false;
	if(host.find(':') != String::npos)
		parseOk = self._sockAddr.parse(host.c_str());
	else
		parseOk = self._sockAddr.parse(host.c_str(), 80);

	if(!parseOk) {
		roLog("error", "Fail to resolve host %s\n", host.c_str());
		return Status::net_resolve_host_fail;
	}

	// Create socket
	roVerify(socket.create(BsdSocket::TCP) == 0);
	socket.setBlocking(false);

	// Make connection
	int ret = socket.connect(self._sockAddr);
	if(ret != 0 && !BsdSocket::inProgress(socket.lastError)) {
		roLog("error", "Connection to %s failed\n", host.c_str());
		return self.setError(Status::net_cannont_connect);
	}

	const char getFmt[] =
		"GET {} HTTP/1.1\r\n"
		"Host: {}\r\n"	// Required for http 1.1
		"User-Agent: The Roar Engine\r\n"
//		"Range: bytes=0-64\r\n"
		"\r\n";

	self._requestStr.clear();
	strFormat(self._requestStr, getFmt, self._path.c_str(), self._host.c_str());

	return _http_sendRequest(s, bytesToRead);
}

static Status _http_sendRequest(IStream& s, roUint64 bytesToRead)
{
	HttpIStream& self = static_cast<HttpIStream&>(s);
	BsdSocket& socket = self._socket;
	String& requestStr = self._requestStr;

	roAssert(!requestStr.isEmpty());

	int ret = socket.send(requestStr.c_str(), requestStr.size());
	if(ret == 0 || (ret < 0 && socket.lastError == ENOTCONN))
		return self.setError(Status::net_not_connected);

	if(ret < 0 && !BsdSocket::inProgress(socket.lastError))
		return self.setError(Status::net_error);

	self._next = _http_readResponse;
	return self.st;
}

static Status _http_readFromSocket(IStream& s, roUint64 bytesToRead)
{
	HttpIStream& self = static_cast<HttpIStream&>(s);
	BsdSocket& socket = self._socket;

	roSize byteSize = 0;
	self.st = roSafeAssign(byteSize, bytesToRead);
	if(!self.st) return self.setError();

	roByte* wPtr = NULL;
	self.st = self._ringBuf.write(byteSize, wPtr);
	if(!self.st) return self.setError();

	int ret = socket.receive(wPtr, byteSize);
	self._ringBuf.commitWrite(ret > 0 ? ret : 0);

	if(ret < 0 && BsdSocket::inProgress(socket.lastError))
		return self.setError(Status::in_progress);
	if(ret < 0)
		return self.setError(Status::net_error);
	if(ret == 0)
		return self.setError(Status::file_ended);

	return self.st;
}

static Status _parseHttpHeader(char* buf, roSize bufSize, roSize& headerSize, IArray<char*>& name, IArray<char*>& value)
{
	// Check if header complete
	static const char headerTerminator[] = "\r\n\r\n";

	const char* messageContent = roStrnStr(buf, bufSize, headerTerminator);
	if(!messageContent)	// We need to read more till we see the header terminator
		return Status::in_progress;

	// Skip the header terminator
	messageContent += 4;
	headerSize = messageContent - buf;

	// Tokenize the string by new line
	for(char* begin = buf, *end = NULL; end + 2 < messageContent; )
	{
		end = roStrStr(buf, messageContent, "\r\n");
		*end = '\0';

		// Split by ':'
		name.pushBack(begin);
		char* p = roStrStr(begin, end, ":");
		if(p) {
			*p = '\0';
			++p;
		}
		value.pushBack(p);

		begin = end += 2;
	}

	if(name.isEmpty())
		return Status::http_error;

	return Status::ok;
}

static Status _http_readResponse(IStream& s, roUint64 bytesToRead)
{
	HttpIStream& self = static_cast<HttpIStream&>(s);
	BsdSocket& socket = self._socket;

	self.st = _http_readFromSocket(s, 1024);
	if(!self.st) return self.setError();

	roSize readSize = 0;
	roByte* rPtr = self._ringBuf.read(readSize);

	roSize headerSize = 0;
	TinyArray<char*, 16> name, value;
	self.st = _parseHttpHeader((char*)rPtr, readSize, headerSize, name, value);
	if(!self.st) return self.setError();

	{	// Parse response
		Regex regex;
		if(!regex.match(name[0], "^HTTP/([\\d\\.]+)[ ]+(\\d+)")) {
			roLog("error", "Invalid http response header: %s\n", name[0]);
			return self.setError(Status::http_error);
		}

		regex.result[2].end = '\0';
		int serverRetCode = roStrToInt32(regex.result[2].begin, 0);

		if(serverRetCode == 200)			// Ok
		{

		}
		else if(serverRetCode == 206)		// Partial Content
		{
			serverRetCode = 206;
		}
		else if(
			serverRetCode == 301 ||			// Moved Permanently
			serverRetCode == 302			// Found (http redirect)
		)
		{
			self._next = _http_connect;
			self.st = Status::in_progress;
		}
		else
		{
			self.st = Status::http_error;
			roLog("error", "Http stream receive server error code '%d'\n", serverRetCode);
		}
	}

	self._ringBuf.commitRead(headerSize);

	if(!self.st) return self.setError();
	return self.st;
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
