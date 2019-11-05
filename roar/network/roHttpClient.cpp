#include "pch.h"
#include "roHttp.h"
#include "roSecureSocket.h"
#include "../base/roCompressedStream.h"
#include "../base/roIOStream.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roStringFormat.h"
#include "../base/roTypeCast.h"

#include "roHttpClientStream.inc"

// Http reference:
// http://en.wikipedia.org/wiki/List_of_HTTP_header_fields
// Proxy reference:
// http://www.mnot.net/blog/2011/07/11/what_proxies_must_do
// http://stackoverflow.com/questions/10876883/implementing-an-http-proxy
// Https proxy captured from Wireshark while tring to connect https://sourceforge.net
// CONNECT mdc-web-tomcat17.ubisoft.org:443 HTTP/1.1\\r\\n
// Mozilla/5.0 (Windows NT 6.1; WOW64; rv:29.0) Gecko/20100101 Firefox/29.0\r\n
// Proxy-Connection: keep-alive\r\n
// Connection: keep-alive\r\n
// Host: mdc-web-tomcat17.ubisoft.org\r\n

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
// HttpClient

HttpClient::HttpClient()
{
	logLevel = 0;
	useHttpCompression = true;
}

static AutoPtr<IStream> _getStream(const HttpResponseHeader& response, const RangedString& preSocketBuf, CoSocket& socket)
{
	AutoPtr<IStream> istream;

	roUint64 contentLength = 0;
	RangedString transferEncoding, tillCloseConnection;

	// Determine the encoding
	if(response.getField(HttpResponseHeader::HeaderField::ContentLength, contentLength)) {
		AutoPtr<HttpClientSizedIStream> s = _allocator.newObj<HttpClientSizedIStream>(socket, clamp_cast<roSize>(contentLength));
		s->begin = (roByte*)preSocketBuf.begin;
		s->end = (roByte*)preSocketBuf.end;
		s->current = s->begin;
		istream = std::move(s);

//		if(logLevel > 0)
//			roLog("info", "HttpClient - Content length: %u\n", contentLength);
	}
	else if(response.getField(HttpResponseHeader::HeaderField::TransferEncoding, transferEncoding)) {
		AutoPtr<HttpClientChunkedIStream> s = _allocator.newObj<HttpClientChunkedIStream>(socket);
		s->begin = (roByte*)preSocketBuf.begin;
		s->end = (roByte*)preSocketBuf.end;
		s->current = s->begin;
		istream = std::move(s);
	}
	else if(response.cmpFieldNoCase(HttpResponseHeader::HeaderField::Connection, "close")) {
		AutoPtr<HttpClientReadTillEndIStream> s = _allocator.newObj<HttpClientReadTillEndIStream>(socket);
		s->begin = (roByte*)preSocketBuf.begin;
		s->end = (roByte*)preSocketBuf.end;
		s->current = s->begin;
		istream = std::move(s);
	}
	else {
		// Create a null stream
		AutoPtr<MemoryIStream> s = _allocator.newObj<MemoryIStream>();
		istream = std::move(s);
	}

	// Any compression?
	RangedString contentEncoding;
	if(response.getField(HttpResponseHeader::HeaderField::ContentEncoding, contentEncoding)) {
		if(contentEncoding.cmpNoCase("gzip") == 0) {
			AutoPtr<GZipIStream> s = _allocator.newObj<GZipIStream>();
			s->init(std::move(istream));
			roAssert(!istream.ptr());
			istream = std::move(s);
		}
	}

	return istream;
}

static roStatus _skipStreamData(const HttpResponseHeader& response, IStream& istream)
{
	roStatus st = roStatus::ok;

	if(response.cmpFieldNoCase(HttpResponseHeader::HeaderField::Connection, "keep-alive")) {
		// Read all the remaining data
		roByte buf[128];
		roUint64 read = 0;
		do {
			st = istream.read(buf, sizeof(buf), read);
		} while(st);

		if(st == roStatus::end_of_data)
			st = roStatus::ok;
	}

	return st;
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

	String protocol, host, resourcePath;

// Loop for location redirection
const roSize maxRedirect = 10;
roSize redirectCount = 0;
while(true) {
	String responseStr;
	roSize contentStartPos = 0;
	bool hasValidFullPath = false;

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

		{	RangedString protocol_, host_, path_;
			hasValidFullPath = splitUrl(RangedString(resourcePath), protocol_, host_, path_);

			if(hasValidFullPath)
				protocol = protocol_;
			else
				protocol = "http";

			// TODO: Check that host == host_?
		}

		// Make sure the resource path is a full path, when we were using proxy
		if(!proxy.isEmpty()) {
			if(!hasValidFullPath) {
				String tmp;
				st = strFormat(tmp, "{}://{}{}", protocol, host, resourcePath);
				if(!st) roEXCP_THROW;
				roSwap(tmp, resourcePath);
			}
		}
	}

	// Parse address string
	SockAddr addr;
	String serverToConnect = host;
	{	bool parseOk = false;

		if(!proxy.isEmpty())
			serverToConnect = proxy;

		if(serverToConnect.find(':') != String::npos)
			parseOk = addr.parse(serverToConnect.c_str());
		else
			parseOk = addr.parse(serverToConnect.c_str(), 80);	// TODO: Assign default port base on protocol

		if(!parseOk) {
			roLog("error", "Fail to resolve server %s\n", serverToConnect.c_str());
			st =  Status::net_resolve_host_fail;
			roEXCP_THROW;
		}
	}

	{	// Look for existing connection suitable for this request
		if(!connectionPool)
			connectionPool = &dummyPool;

		if(logLevel > 0)
			roLog("info", "HttpClient - Connecting to %s\n", serverToConnect.c_str());

		st = connectionPool->getConnection(addr, connection);
		if(!st) roEXCP_THROW;

		roAssert(connection);
		connection->removeThis();
	}

	// Handling with https
	if(roStrCaseCmp(protocol.c_str(), "https") == 0)
	{
		// Send https connect request to proxy
		if(!proxy.isEmpty()) {
			String requestStr;
			st = strFormat(requestStr, "CONNECT {} HTTP/1.1\r\nHost: {}\r\n\r\n", host, host);
			if(!st) roEXCP_THROW;

			roSize len = requestStr.size();
			st = connection->socket.send(requestStr.c_str(), len);
			if(!st) roEXCP_THROW;
		}

		st = secureSocketHandshake(connection->socket);
		if(!st) roEXCP_THROW;
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

	AutoPtr<IStream> istream;

	// Decode status code
	switch(statusCode)
	{
	case 100:		// Continue
	case 200:		// OK
	case 400:		// Bad request
	case 401:		// Unauthorized
	case 404:		// Not found
	case 416:		// Requested Range not satisfiable
	{
		istream = std::move(_getStream(
			response,
			RangedString(responseStr.c_str() + contentStartPos, responseStr.end()),
			connection->socket
		));

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

		String str = redirectLocation.toString();
		if(logLevel > 0)
			roLog("info", "HttpClient - Redirecting to: %s\n", str.c_str());

		st = header.make(HttpRequestHeader::Method::Get, str.c_str());
		if(!st) roEXCP_THROW;

		++redirectCount;
		if(redirectCount > maxRedirect) {
			st = roStatus::http_max_redirect_reached;
			roEXCP_THROW;
		}

		// Release connection back to pool
		if(response.cmpFieldNoCase(HttpResponseHeader::HeaderField::Connection, "close")) {
			// Can be deleted since server will close this connection and while we are redirecting to another location (new connection needed)
			roDelete(connection);
		}
		else {
			connectionPool->connections.insert(*connection);

			istream = std::move(_getStream(
				response,
				RangedString(responseStr.c_str() + contentStartPos, responseStr.end()),
				connection->socket
			));

			st = _skipStreamData(response, *istream);
		}

		if(!st) roEXCP_THROW;

		// Back to the start of the location redirect loop
		continue;
	}	break;
	default:
		break;
	}

	// For keep-alive to work, we need to read till end of the http response
	// if the client callback didn't do so.
	st = _skipStreamData(response, *istream);
	if(!st) roEXCP_THROW;

	break;
}	// End of location redirection loop

roEXCP_CATCH
roEXCP_END
	// Release connection back to pool
	if(connection)
		connectionPool->connections.insert(*connection);

	return st;
}

}	// namespace ro
