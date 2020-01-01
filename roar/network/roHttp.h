#ifndef __network_roHttp_h__
#define __network_roHttp_h__

#include "roSocket.h"
#include "../base/roLinkList.h"
#include "../base/roMap.h"
#include "../base/roMemory.h"
#include "../base/roString.h"
#include <functional>

namespace ro {

enum class HttpVersion
{
	Unknown = 0,
	v0_9,
	v1_0,
	v1_1,
	v2_0,
};

struct HttpRequestHeader
{
	// http://www.tutorialspoint.com/http/http_methods.htm
	enum class Method {
		Get = 0,
		Head,
		Post,
		Put,
		Delete,
		Connect,
		Options,
		Trace,
		Invalid,
	};

	enum class HeaderField {
		Accept = 0,				// Accept: text/plain
		AcceptCharset,			// Accept-Charset: utf-8
		AcceptEncoding,			// Accept-Encoding: gzip, deflate
		AcceptLanguage,			// Accept-Language: en-US
		AcceptDatetime,			// Accept-Datetime: Thu, 31 May 2007 20:35:00 GMT
		Authorization,			// Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
		CacheControl,			// Cache-Control: no-cache
		Connection,				// Connection: keep-alive
		Cookie,					// Cookie: $Version=1; Skin=new;
		ContentLength,			// Content-Length: 348
		ContentMD5,				// Content-MD5: Q2hlY2sgSW50ZWdyaXR5IQ==
		ContentType,			// Content-Type: application/x-www-form-urlencoded
		Date,					// Date: Tue, 15 Nov 1994 08:12:31 GMT
		Expect,					// Expect: 100-continue
		From,					// From: user@example.com
		Host,					// Host: en.wikipedia.org:80
		IfMatch,				// If-Match: ETagValue
		IfNoneMatch,			// If-None-Match: ETagValue
		MaxForwards,			// Max-Forwards: 10
		Method,					// GET / HEAD / POST / PUT / DELETE / CONNECT / OPTIONS or TRACE
		Origin,					// Origin: http://www.example-social-network.com
		Resource,				// GET /index.html HTTP/1.1
		Pragma,					// Pragma: no-cache
		ProxyAuthorization,		// Proxy-Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
		Range,					// Range: bytes=500-999
		Referer,				// Referer: http://en.wikipedia.org/wiki/Main_Page
		SecWebSocketVersion,	// Sec-WebSocket-Version: 13
		SecWebSocketkey,		// Sec-WebSocket-Key: q4xkcO32u266gldTuKaSOw==
		Upgrade,				// Upgrade: websocket
		UserAgent,				// User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:12.0) Gecko/20100101 Firefox/21.0
		Version,				// HTTP/1.1 206 Partial Content
		Via						// Via: 1.0 fred, 1.1 example.com (Apache/1.1)
	};

	/// Functions for composing requestString
	Status		make			(Method method, const char* fullUrl);
	Status		addField		(const char* field, const char* value);
	Status		addField		(HeaderField field, const char* value);
	Status		addField		(HeaderField field, roUint64 value);
	Status		addField		(HeaderField field, roUint64 value1, roUint64 value2);

	void		removeField		(const char* field);
	void		removeField		(HeaderField field);

	/// Parse and get information from requestString
	HttpVersion	getVersion  	() const;
	Method  	getMethod		() const;

	bool		getField		(const char* option, String& value) const;
	bool		getField		(const char* option, RangedString& value) const;
	bool		getField		(HeaderField option, String& value) const;
	bool		getField		(HeaderField option, RangedString& value) const;

	bool		cmpFieldNoCase	(HeaderField option, const char* value) const;
	String string;
};	// HttpRequestHeader

struct HttpResponseHeader
{
	enum class ResponseCode {
		Continue,					// 100 - Everything so far is OK, client should continue the request
		SwitchingProtocol,			// 101 - Response to an Upgrade
		OK,							// 200 - The request has succeeded
		NotModified,				// 304 - Not modified
		NotFound,					// 404 - The server can not find requested resource
		InternalServerError,		// 500 - The server has encountered a situation it doesn't know how to handle
	};

	enum class HeaderField {
		AccessControlAllowOrigin,	// Access-Control-Allow-Origin: *
		AcceptRanges,				// Accept-Ranges: bytes
		Age,						// Age: 12
		Allow,						// Allow: GET, HEAD
		CacheControl,				// Cache-Control: max-age=3600
		Connection,					// Connection: close
		ContentEncoding,			// Content-Encoding: gzip
		ContentLanguage,			// Content-Language: da
		ContentLength,				// Content-Length: 348
		ContentLocation,			// Content-Location: /index.htm
		ContentMD5,					// Content-MD5: Q2hlY2sgSW50ZWdyaXR5IQ==
		ContentDisposition,			// Content-Disposition: attachment; filename="fname.ext"
		ContentRange,				// Content-Range: bytes 21010-47021/47022
		ContentType,				// Content-Type: text/html; charset=utf-8
		Date,						// Date: Tue, 15 Nov 1994 08:12:31 GMT
		ETag,						// ETag: "737060cd8c284d8af7ad3082f209582d"
		Expires,					// Expires: Thu, 01 Dec 1994 16:00:00 GMT
		LastModified,				// Last-Modified: Tue, 15 Nov 1994 12:45:26 +0000
		Link,						// Link: </feed>; rel="alternate"
		Location,					// Location: http://www.w3.org/pub/WWW/People.html
		P3P,						// Will not support
		Pragma,						// Pragma: no-cache
		ProxyAuthenticate,			// Proxy-Authenticate: Basic
		Refresh,					// Refresh: 5; url=http://www.w3.org/pub/WWW/People.html
		RetryAfter,					// Example 1: Retry-After: 120, Example 2: Retry-After: Fri, 04 Nov 2014 23:59:59 GMT
		SecWebSocketAccept,			// Sec-WebSocket-Accept: fA9dggdnMPU79lJgAE3W4TRnyDM=
		Server,						// Server: Apache/2.4.1 (Unix)
		SetCookie,					// Set-Cookie: UserID=JohnDoe; Max-Age=3600; Version=1
		Status,						// Status: 200 OK
		StrictTransportSecurity,	// Strict-Transport-Security: max-age=16070400; includeSubDomains
		Trailer,					// Trailer: Max-Forwards
		TransferEncoding,			// Transfer-Encoding: chunked
		Upgrade,					// Upgrade: websocket
		Vary,						// Vary: *
		Version,					// HTTP/1.1 206 Partial Content
		Via,						// Via: 1.0 fred, 1.1 example.com (Apache/1.1)
		WWWAuthenticate,			// WWW-Authenticate: Basic
	};

	/// Functions for composing responseString
	Status		make			(ResponseCode responseCode);
	Status		addField		(const char* field, const char* value);
	Status		addField		(HeaderField field, const char* value);
	Status		addField		(HeaderField field, roUint64 value);

	/// Parse and get information from responseString
	bool		getField		(const char* option, String& value) const;
	bool		getField		(const char* option, RangedString& value) const;
	bool		getField		(HeaderField option, String& value) const;
	bool		getField		(HeaderField option, RangedString& value) const;
	bool		getField		(HeaderField option, roUint64& value) const;
	bool		getField		(HeaderField option, roUint64& value1, roUint64& value2, roUint64& value3) const;

	bool		cmpFieldNoCase	(HeaderField option, const char* value) const;

	String string;
};	// HttpResponseHeader

roStatus splitUrl(const RangedString& url, RangedString& protocol, RangedString& host, RangedString& path);

struct HttpOptions
{
	enum class Option {
		MaxUrlSize,
		MaxHeaderSize,
	};
};	// HttpOptions

struct IStream;

struct HttpConnection : public MapNode<SockAddr, HttpConnection>
{
	CoSocket socket;
};	// HttpConnection

struct HttpConnections
{
	roStatus getConnection(const SockAddr& address, HttpConnection*& outConnection);
	Map<HttpConnection> connections;
};	// HttpConnections

struct HttpClient
{
	HttpClient();
	~HttpClient() {}

	typedef	roStatus	(*ReplyFunc)(const HttpResponseHeader& response, IStream& body, void* userPtr);
			roStatus	request		(const HttpRequestHeader& header, ReplyFunc replyFunc, void* userPtr=NULL, HttpConnections* connectionPool=NULL);

	roUint8	logLevel;	// 0 for no logging, larger for more
	bool	useHttpCompression;
	String	proxy;
};	// HttpClient

struct OStream;

struct HttpServer
{
	HttpServer();
	~HttpServer();

	struct Connection : public ro::ListNode<HttpServer::Connection>
	{
		Connection();

		roStatus	response(HttpResponseHeader& header);
		roStatus	response(HttpResponseHeader& header, OStream*& os);
		roStatus	response(HttpResponseHeader& header, OStream*& os, roSize contentSize);

		roStatus	_processHeader(HttpRequestHeader& header);

		HttpVersion httpVersion;
		bool		keepAlive = false;
		bool		supportGZip = false;
		bool		supportChunkEncoding = false;

		CoSocket	socket;
		AutoPtr<OStream> oStream;
	};	// HttpServer::Connection

	Status	init();

	typedef std::function<roStatus(Connection& connection, HttpRequestHeader& request)> OnRequest;
	roStatus	start();

	/// Settings
	roUint16 port = 80;
	unsigned backlog = 256;	// The backlog pass to CoSocket::listen
	float keepAliveTimeout = 15;

	/// Callbacks
	OnRequest onRequest;
	OnRequest onWebScoketRequest;

	/// Web socket helper functions
	roStatus webSocketResponse(Connection& connection, HttpRequestHeader& request);	// To be called in onWebScoketRequest()

	struct WebsocketFrame
	{
		// Reference:
		// https://sookocheff.com/post/networking/how-do-websockets-work
		enum class Opcode {
			Continue	= 0x00,	// This frame continues the payload from the previous frame
			Text		= 0x01,	// Denotes a text frame, UTF-8 encoded
			Binary		= 0x02,	// Denotes a binary frame. Binary frames are delivered unchanged
			Close		= 0x08,	// Denotes the client wishes to close the connection
			Ping		= 0x09,	// A ping frame. Serves as a heartbeat mechanism ensuring the connection is still alive. The receiver must respond with a pong frame
			Pong		= 0x0a,	// A pong frame. Serves as a heartbeat mechanism ensuring the connection is still alive. The receiver must respond with a ping frame
		};

		roStatus read(void* buf, roSize& len);

		bool	isLastFrame;
		Opcode	opcode;
		roSize	getReamin() const { return _remain; }

	// Private
		roUint8	_mask[4];
		roSize	_maskIdx = 0;
		roSize	_remain = 0;
		Connection* _connection = NULL;
	};
	roStatus readWebSocketFrame(Connection& connection, WebsocketFrame& is);
	roStatus writeWebSocketFrame(Connection& connection, WebsocketFrame::Opcode opcode, const void* data, roSize size, bool isLastFrame=true);

	LinkList<Connection> activeConnections;
	LinkList<Connection> pooledConnections;

// Private
	CoSocket	_socketListen;
};	// HttpServer

}	// namespace ro

#endif	// __network_roHttp_h__
