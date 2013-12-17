#include "pch.h"
#include "roHttp.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roStringFormat.h"
#include "../base/roTypeCast.h"
#include "../base/roUtility.h"

namespace ro {

//////////////////////////////////////////////////////////////////////////
// HttpRequest

Status HttpRequest::addField(const char* option, const char* value)
{
	return strFormat(requestString, "{}: {}\r\n", option, value);
}

Status HttpRequest::addField(HeaderField::Enum option, const char* value)
{
	switch(option) {
	case HeaderField::Accept:
		return addField("Accept", value);
	case HeaderField::AcceptCharset:
		return addField("Accept-Charset", value);
	case HeaderField::AcceptEncoding:
		return addField("Accept-Encoding", value);
	case HeaderField::AcceptLanguage:
		return addField("Accept-Language", value);
	case HeaderField::Authorization:
		return addField("Authorization", value);
	case HeaderField::CacheControl:
		return addField("Cache-Control", value);
	case HeaderField::Connection:
		return addField("Connection", value);
	case HeaderField::Cookie:
		return addField("Cookie", value);
	case HeaderField::ContentMD5:
		return addField("Content-MD5", value);
	case HeaderField::ContentType:
		return addField("Content-Type", value);
	case HeaderField::Expect:
		return addField("Expect", value);
	case HeaderField::From:
		return addField("From", value);
	case HeaderField::Host:
		return addField("Host", value);
	case HeaderField::Origin:
		return addField("Origin", value);
	case HeaderField::Pragma:
		return addField("Pragma", value);
	case HeaderField::ProxyAuthorization:
		return addField("Proxy-Authorization", value);
	case HeaderField::Referer:
		return addField("Referer", value);
	case HeaderField::UserAgent:
		return addField("UserAgent", value);
	case HeaderField::Via:
		return addField("Via", value);
	}

	return Status::invalid_parameter;
}

Status HttpRequest::addField(HeaderField::Enum option, roUint64 value)
{
	switch(option) {
	case HeaderField::ContentLength:
		return strFormat(requestString, "{}: {}\r\n", "Content-Length", value);
	case HeaderField::MaxForwards:
		return strFormat(requestString, "{}: {}\r\n", "Max-Forwards", value);
	case HeaderField::Range:
		return strFormat(requestString, "Range: bytes={}-\r\n", value);
	}

	return Status::invalid_parameter;
}

Status HttpRequest::addField(HeaderField::Enum option, roUint64 value1, roUint64 value2)
{
	switch(option) {
	case HeaderField::Range:
		return strFormat(requestString, "Range: bytes={}-{}\r\n", value1, value2);
	}

	return Status::invalid_parameter;
}


//////////////////////////////////////////////////////////////////////////
// HttpResponse

bool HttpResponse::getField(const char* option, RangedString& value)
{
	char* begin = responseString.c_str();

	while(begin < responseString.end())
	{
		// Tokenize the string by new line
		char* end = roStrStr(begin, responseString.end(), "\r\n");

		if(begin == end)
			break;

		// Split by ':'
		char* p = roStrStr(begin, end, ":");

		if(p && roStrStrCase(begin, p, option)) {
			++p;

			// Skip space
			while(*p == ' ') ++p;

			value = RangedString(p, end);
			return true;
		}

		begin = end + 2;
	}

	return false;
}

bool HttpResponse::getField(HeaderField::Enum option, RangedString& value)
{
	switch(option) {
	case HeaderField::AccessControlAllowOrigin:
		return getField("Access-Control-Allow-Origin", value);
	case HeaderField::AcceptRanges:
		return getField("Accept-Ranges", value);
	case HeaderField::Allow:
		return getField("Allow", value);
	case HeaderField::CacheControl:
		return getField("Cache-Control", value);
	case HeaderField::Connection:
		return getField("Connection", value);
	case HeaderField::ContentEncoding:
		return getField("Content-Encoding", value);
	case HeaderField::ContentLanguage:
		return getField("Content-Language", value);
	case HeaderField::ContentLocation:
		return getField("Content-Location", value);
	case HeaderField::ContentMD5:
		return getField("Content-MD5", value);
	case HeaderField::ContentDisposition:
		return getField("Content-Disposition", value);
	case HeaderField::ContentType:
		return getField("Content-Type", value);
	case HeaderField::ETag:
		return getField("ETag", value);
	case HeaderField::Link:
		return getField("Link", value);
	case HeaderField::Location:
		return getField("Location", value);
	case HeaderField::Pragma:
		return getField("Pragma", value);
	case HeaderField::ProxyAuthenticate:
		return getField("Proxy-Authenticate", value);
	case HeaderField::Refresh:
		return getField("Refresh", value);
	case HeaderField::RetryAfter:
		return getField("RetryAfter", value);
	case HeaderField::Server:
		return getField("Server", value);
	case HeaderField::SetCookie:
		return getField("Set-Cookie", value);
	case HeaderField::Status:
		return getField("Status", value);
	case HeaderField::StrictTransportSecurity:
		return getField("Strict-Transport-Security", value);
	case HeaderField::Trailer:
		return getField("Trailer", value);
	case HeaderField::TransferEncoding:
		return getField("TransferEncoding", value);
	case HeaderField::Vary:
		return getField("Vary", value);
	case HeaderField::Version:
		{	Regex regex;
			regex.match(responseString.c_str(), "^HTTP/([\\d\\.]+)", "i");
			return regex.getValue(1, value);
		}
		return getField("Vary", value);
	case HeaderField::Via:
		return getField("Via", value);
	case HeaderField::WWWAuthenticate:
		return getField("WWWAuthenticate", value);
	}

	return false;
}

bool HttpResponse::getField(HeaderField::Enum option, roUint64& value)
{
	Regex regex;
	bool ok = false;
	RangedString str;

	switch(option) {
	case HeaderField::Age:
		ok = getField("Age", str);
		break;
	case HeaderField::ContentLength:
		ok = getField("Content-Length", str);
		break;
	case HeaderField::Status:
		ok = regex.match(responseString.c_str(), "^HTTP/[\\d\\.]+[ ]+(\\d\\d\\d)", "i");
		if(regex.getValue(1, value))
			return true;

		ok = getField("Status", str);
		break;
	}

	if(!ok)
		return false;

	return roStrTo(str.begin, str.size(), value);
}

bool HttpResponse::getField(HeaderField::Enum option, roUint64& value1, roUint64& value2, roUint64& value3)
{
	Regex regex;
	RangedString str;

	switch(option) {
	case HeaderField::ContentRange:
		getField("Content-Range", str);
		regex.match(str, "^\\s*bytes\\s+([\\d]+)\\s*-\\s*([\\d]+)\\s*/([\\d]+)");
		if(!regex.getValue(1, value1)) return false;
		if(!regex.getValue(2, value2)) return false;
		return regex.getValue(3, value3);
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
// HttpClient

HttpClient::HttpClient()
{
	debug = false;
	roVerify(BsdSocket::initApplication() == 0);
}

HttpClient::~HttpClient()
{
	roVerify(BsdSocket::closeApplication() == 0);
}

void HttpClient::Response::reset()
{
	statusCode = 0;
	keepAlive.name = RangedString();
	location = RangedString();
	transferEncoding = RangedString();
	contentType = RangedString();
	contentRange = RangedString();
	string.clear();
}

Status HttpClient::performGet(const char* resourceName)
{
	const char getFmt[] =
		"GET {} HTTP/1.1\r\n"
		"Host: {}\r\n"	// Required for http 1.1
		"Connection: keep-alive\r\n"
		"User-Agent: The Roar Engine\r\n"
		"Accept-Encoding: deflate\r\n"
//		"Range: bytes=0-128\r\n"
		"\r\n";

	return strFormat(requestString, getFmt, resourceName, host.c_str());
}

Status HttpClient::connect(const char* hostAndPort)
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
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	requestString.clear();

	_nextFunc = &HttpClient::_funcWaitConnect;
	return (this->*_nextFunc)();
}

Status HttpClient::_funcWaitConnect()
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
		roLog("debug", "HttpClient: host connected\n");

roEXCP_CATCH
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	_nextFunc = &HttpClient::_funcReadResponse;
	return Status::in_progress;
}

Status HttpClient::_funcSendRequest()
{
	Status st;

roEXCP_TRY
	if(requestString.isEmpty())
		return Status::ok;

	if(_nextFunc == &HttpClient::_funcWaitConnect)
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
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::_readFromSocket(roUint64 bytesToRead)
{
	roSize byteSize = 0;
	Status st = roSafeAssign(byteSize, bytesToRead);
	if(!st) return st;

	roByte* wPtr = NULL;
	st = ringBuf.write(byteSize, wPtr);
	if(!st) return st;

	int ret = _socket.receive(wPtr, byteSize);
	ringBuf.commitWrite(ret > 0 ? ret : 0);

	if(ret < 0 && BsdSocket::inProgress(_socket.lastError))
		return Status::in_progress;
	if(ret < 0)
		return Status::net_error;
	if(ret == 0)
		return Status::file_ended;

	return st;
}

Status HttpClient::Response::parse(const RangedString& str)
{
	reset();
	string = str;

	char* begin = string.c_str();

	// Parse the start line
	{	char* end = roStrStr(begin, string.end(), "\r\n");
		Regex regex;
		if(!regex.match(string.c_str(), "^HTTP/([\\d\\.]+)[ ]+(\\d+)"))
			return Status::http_header_error;

		statusCode = regex.getValueWithDefault(2, roUint16(0));
		begin = end + 2;
	}

	// Tokenize the string by new line
	while(begin < string.end())
	{
		// Split by ':'
		char* end = roStrStr(begin, string.end(), "\r\n");
		char* p = roStrStr(begin, end, ":");

		if(begin == end)
			break;

		if(!p)
			return Status::http_header_error;

		RangedString name = RangedString(begin, p);
		RangedString value = RangedString(++p, end);

		begin = end + 2;
	}

	return Status::ok;
}

Status HttpClient::_funcReadResponse()
{
	Status st;

roEXCP_TRY
	st = _readFromSocket(1024);

	if(st == Status::in_progress)
		return st;

	if(!st) roEXCP_THROW;

	roSize readSize = 0;
	char* rPtr = (char*)ringBuf.read(readSize);

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

	ringBuf.commitRead(headerSize);

	// Parse the header response into individual elements
	st = response.parse(RangedString(rPtr, rPtr + headerSize));
	if(!st) roEXCP_THROW;

	if(debug)
		roLog("debug", "HttpClient received response:\n%s\n", response.string.c_str());

roEXCP_CATCH
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	_nextFunc = &HttpClient::_funcProcessResponse;
	return (this->*_nextFunc)();
}

Status HttpClient::_parseResponseHeader()
{
	return Status::ok;
}

Status HttpClient::_funcProcessResponse()
{
	Status st;

roEXCP_TRY
	switch(response.statusCode)
	{
	case 200:	// Ok
		break;
	case 206:	// Partial Content
		break;
	case 301:	// Moved Permanently
	case 302:	// Found (http redirect)
//		return connect();
		break;
	case 416:	// Requested Range not satisfiable
		break;
	default:
		break;
	}

roEXCP_CATCH
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::_funcReadBody()
{
	Status st;

roEXCP_TRY
	st = _readFromSocket(1024);

	if(st == Status::in_progress)
		return st;

	if(!st) roEXCP_THROW;

	roSize readSize = 0;
	char* rPtr = (char*)ringBuf.read(readSize);

roEXCP_CATCH
	_nextFunc = &HttpClient::_funcAborted;
	return st;

roEXCP_END
	return (this->*_nextFunc)();
}

Status HttpClient::update()
{
	Status st;
	st = _funcSendRequest();
	if(!st && st != Status::in_progress) return st;

	st = (this->*_nextFunc)();
	return st;
}

Status HttpClient::_funcAborted()
{
	return Status::ok;
}

Status HttpClient::splitUrl(const char* url, RangedString& protocol, RangedString& host, RangedString& path)
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
