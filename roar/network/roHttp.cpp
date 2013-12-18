#include "pch.h"
#include "roHttp.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roTypeCast.h"
#include "../base/roUtility.h"

namespace ro {

struct IFifoWrite
{
	virtual roStatus	requestWrite		(roSize maxSizeToWrite, roByte*& outWritePtr) = 0;
	virtual void		commitWrite			(roSize written) = 0;
	virtual bool		keepWrite			() const = 0;
};

struct IFifoRead
{
	virtual roStatus	requestRead			(roSize& outReadSize, roByte*& outReadPtr) = 0;
	virtual void		commitRead			(roSize read) = 0;
	virtual void		contentReadBegin	() = 0;
};

struct SizedFifoRingBuf : public IFifoWrite, public IFifoRead
{
	SizedFifoRingBuf();

	override roStatus	requestWrite		(roSize maxSizeToWrite, roByte*& outWritePtr)	{ return ringBuffer->write(maxSizeToWrite, outWritePtr); }
	override void		commitWrite			(roSize written);
	override bool		keepWrite			() const										{ return !contentBegin || bytesAlreadyWritten < expectedEnd; }
	override void		contentReadBegin	()												{ contentBegin = true; expectedEnd += bytesAlreadyRead; }
	override roStatus	requestRead			(roSize& outReadSize, roByte*& outReadPtr);
	override void		commitRead			(roSize read);

	RingBuffer*	ringBuffer;
	bool		contentBegin;
	roUint64	expectedEnd;
	roUint64	bytesAlreadyWritten;
	roUint64	bytesAlreadyRead;
};	// SizedFifoRingBuf

SizedFifoRingBuf::SizedFifoRingBuf()
	: ringBuffer(NULL)
	, contentBegin(false)
	, expectedEnd(0)
	, bytesAlreadyWritten(0)
	, bytesAlreadyRead(0)
{}

void SizedFifoRingBuf::commitWrite(roSize written)
{
	ringBuffer->commitWrite(written);
	bytesAlreadyWritten += written;
}

roStatus SizedFifoRingBuf::requestRead(roSize& outReadSize, roByte*& outReadPtr)
{
	roAssert(bytesAlreadyRead <= expectedEnd);

	if(contentBegin && bytesAlreadyRead >= expectedEnd)
		return Status::end_of_data;

	outReadPtr = ringBuffer->read(outReadSize);
	return Status::ok;
}

void SizedFifoRingBuf::commitRead(roSize read)
{
	ringBuffer->commitRead(read);
	bytesAlreadyRead += read;
}

//////////////////////////////////////////////////////////////////////////
// HttpClient

struct HttpClient::Connection
{
	Connection();

	Status		connect(const char* hostAndPort);
	Status		update();

	Status		_readFromSocket(roUint64 bytesToRead);
	Status		_parseResponseHeader();

	Status		_funcWaitConnect();
	Status		_funcSendRequest();
	Status		_funcReadResponse();
	Status		_funcProcessResponse();
	Status		_funcReadBody();
	Status		_funcIdle();
	Status		_funcAborted();

	typedef Status (Connection::*NextFunc)();
	NextFunc	_nextFunc;

	bool		debug;
	String		host;
	String		requestString;

	bool		_chunked;
	roUint64	_contentLength;

	BsdSocket	_socket;
	SockAddr	_sockAddr;
	Request*	_request;

	IFifoWrite*	_iWrite;
	IFifoRead*	_iRead;

	SizedFifoRingBuf _normalDecoder;

	RingBuffer	ringBuf;
};	// HttpClient::Connection

HttpClient::Request::Request()
	: httpClient(NULL)
	, connection(NULL)
{
}

Status HttpClient::Request::update()
{
	return connection->update();
}

Status HttpClient::Request::requestRead(roSize& outReadSize, roByte*& outReadPtr)
{
	Status st = connection->_iRead->requestRead(outReadSize, outReadPtr);
	return st;
}

void HttpClient::Request::commitRead(roSize read)
{
	return connection->_iRead->commitRead(read);
}

void HttpClient::Request::removeThis()
{
	httpClient = NULL;
	connection = NULL;
}

HttpClient::Connection::Connection()
	: _request(NULL)
	, _iWrite(NULL)
	, _iRead(NULL)
{
	_normalDecoder.ringBuffer = &ringBuf;

	_iWrite = &_normalDecoder;
	_iRead = &_normalDecoder;
}

Status HttpClient::Connection::update()
{
	Status st;
	st = _funcSendRequest();
	if(!st && st != Status::in_progress) return st;

	st = (this->*_nextFunc)();
	return st;
}

Status HttpClient::Connection::connect(const char* hostAndPort)
{
	Status st;

roEXCP_TRY
	roVerify(_socket.close() == 0);

	// Create socket
	roVerify(_socket.create(BsdSocket::TCP) == 0);
	_socket.setBlocking(false);

	// NOTE: Currently this host resolving operation is blocking
	host = hostAndPort;
	bool parseOk = false;
	if(host.find(':') != String::npos)
		parseOk = _sockAddr.parse(host.c_str());
	else
		parseOk = _sockAddr.parse(host.c_str(), 80);

	if(!parseOk) {
		roLog("error", "Fail to resolve host %s\n", host.c_str());
		st =  Status::net_resolve_host_fail;
		roEXCP_THROW;
	}

	// Make connection
	int ret = _socket.connect(_sockAddr);

	if(ret != 0 && !BsdSocket::inProgress(_socket.lastError)) {
		roLog("error", "Connection to %s failed\n", host.c_str());
		st = Status::net_cannont_connect;
		roEXCP_THROW;
	}

	if(debug)
		roLog("debug", "HttpClient: connecting to %s\n", host.c_str());

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	requestString.clear();

	_nextFunc = &Connection::_funcWaitConnect;
	return (this->*_nextFunc)();
}

Status HttpClient::Connection::_funcWaitConnect()
{
	Status st;

roEXCP_TRY
	bool readable = false;
	bool writable = true;
	bool hasError = true;
	int ret = _socket.select(readable, writable, hasError);
	(void)ret;

	if(!writable && !hasError)
		return Status::in_progress;
	else if(!writable && hasError) {
		roLog("error", "Connection to %s failed\n", host.c_str());
		st = Status::net_cannont_connect;
		roEXCP_THROW;
	}

	if(debug)
		roLog("debug", "HttpClient: host %s connected\n", host.c_str());

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	_nextFunc = &Connection::_funcReadResponse;
	return Status::in_progress;
}

Status HttpClient::Connection::_funcSendRequest()
{
	Status st;

roEXCP_TRY
	if(requestString.isEmpty())
		return Status::ok;

	if(_nextFunc == &Connection::_funcWaitConnect)
		return Status::in_progress;

	int ret = _socket.send(requestString.c_str(), requestString.size());
	if(ret < 0 && !BsdSocket::inProgress(_socket.lastError)) {
		st = Status::net_error;
		roEXCP_THROW;
	}

	if(debug)
		roLog("debug", "HttpClient sending request:\n%s\n", requestString.c_str());

	requestString.erase(0, ret);

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::Connection::_readFromSocket(roUint64 bytesToRead)
{
	roSize byteSize = 0;
	Status st = roSafeAssign(byteSize, bytesToRead);
	if(!st) return st;

	roByte* wPtr = NULL;
	st = _iWrite->requestWrite(byteSize, wPtr);
	if(!st) return st;

	int ret = _socket.receive(wPtr, byteSize);
	_iWrite->commitWrite(ret > 0 ? ret : 0);

	if(ret < 0 && BsdSocket::inProgress(_socket.lastError))
		return Status::in_progress;
	if(ret < 0)
		return Status::net_error;
	if(ret == 0)
		return Status::file_ended;

	return st;
}

Status HttpClient::Connection::_funcReadResponse()
{
	Status st;

roEXCP_TRY
	st = _readFromSocket(1024);

	if(st == Status::in_progress)
		return st;

	if(!st) roEXCP_THROW;

	roSize readSize = 0;
	roByte* rBytePtr = NULL;
	st = _iRead->requestRead(readSize, rBytePtr);
	if(!st) roEXCP_THROW;

	char* rPtr = (char*)rBytePtr;

	// Check if header complete
	static const char headerTerminator[] = "\r\n\r\n";

	const char* messageContent = roStrnStr(rPtr, readSize, headerTerminator);

	// We need to read more till we see the header terminator
	if(!messageContent) {
		if(ringBuf.totalReadable() > _maxHeaderSize) {
			st = Status::http_header_error;
			roEXCP_THROW;
		}

		return Status::in_progress;
	}

	// Skip the header terminator
	messageContent += 4;
	roSize headerSize = messageContent - rPtr;

	_request->responseHeader.string.assign(rPtr, headerSize);
	_iRead->commitRead(headerSize);
	_iRead->contentReadBegin();
	ringBuf.compactReadBuffer();	// NOTE: Not really necessary

	if(debug)
		roLog("debug", "HttpClient received response:\n%s\n", _request->responseHeader.string.c_str());

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	_nextFunc = &Connection::_funcProcessResponse;
	return (this->*_nextFunc)();
}

Status HttpClient::Connection::_funcProcessResponse()
{
	Status st;

roEXCP_TRY
	roUint64 statusCode = 0;
	if(!_request->responseHeader.getField(HttpResponseHeader::HeaderField::Status, statusCode)) {
		st = Status::http_header_error;
		roEXCP_THROW;
	}

	RangedString rangedString;

	switch(statusCode)
	{
	case 200:	// Ok
		_contentLength = 0;
		if(_request->responseHeader.getField(HttpResponseHeader::HeaderField::ContentLength, _contentLength)) {
			_iWrite = &_normalDecoder;
			_iRead = &_normalDecoder;
			_normalDecoder.expectedEnd += _contentLength;
		}

		_request->responseHeader.getField(HttpResponseHeader::HeaderField::ContentEncoding, rangedString);

		_nextFunc = &Connection::_funcReadBody;
		break;
	case 206:	// Partial Content
		break;
	case 301:	// Moved Permanently
	case 302:	// Found (http redirect)
	{	_request->responseHeader.getField(HttpResponseHeader::HeaderField::Location, rangedString);
		RangedString protocol, host, path;
		st = HttpClient::splitUrl(rangedString, protocol, host, path);
		if(!st)
			roEXCP_THROW;

		return connect(host.toString().c_str());
	}	break;
	case 404:	// Not found
		_nextFunc = &Connection::_funcReadBody;
		break;
	case 416:	// Requested Range not satisfiable
		break;
	default:
		break;
	}

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::Connection::_funcReadBody()
{
	Status st;

roEXCP_TRY
	if(!_iWrite->keepWrite()) {
		_nextFunc = &Connection::_funcIdle;
		return (this->*_nextFunc)();
	}

	st = _readFromSocket(1024);

	if(st == Status::in_progress)
		return st;

	if(!st) roEXCP_THROW;

roEXCP_CATCH
	_nextFunc = &Connection::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::Connection::_funcIdle()
{
	return Status::ok;
}

Status HttpClient::Connection::_funcAborted()
{
	return Status::ok;
}

HttpClient::HttpClient()
{
	debug = false;
	roVerify(BsdSocket::initApplication() == 0);
}

HttpClient::~HttpClient()
{
	roVerify(BsdSocket::closeApplication() == 0);
}

Status HttpClient::perform(Request& request, const HttpRequestHeader& requestHeader)
{
	RangedString host, resource;
	if(!requestHeader.getField(HttpRequestHeader::HeaderField::Host, host))
		return Status::http_header_error;

	if(!requestHeader.getField(HttpRequestHeader::HeaderField::Resource, resource))
		return Status::http_header_error;

	// Look for existing connection suitable for this request
	Connection* connection = NULL;
	Status st = _getConnection(String(host).c_str(), connection);

	if(st || st == Status::in_progress) {
		roAssert(connection);
		request.connection = connection;
		connection->_request = &request;
		connection->requestString = requestHeader.string;
		connection->requestString += "\r\n";
	}

	return st;
}

Status HttpClient::_getConnection(const char* hostAndPort, Connection*& connection)
{
	connection = new Connection;

	return connection->connect(hostAndPort);
}

Status HttpClient::splitUrl(const RangedString& url, RangedString& protocol, RangedString& host, RangedString& path)
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
