#ifndef __network_roHttp_h__
#define __network_roHttp_h__

#include "roSocket.h"
#include "../base/roLinkList.h"
#include "../base/roMap.h"
#include "../base/roRingBuffer.h"
#include "../base/roString.h"

namespace ro {

struct HttpRequestHeader
{
	// http://www.tutorialspoint.com/http/http_methods.htm
	struct Method { enum Enum {
		Get,
		Head, 
		Post,
		Put,
		Delete,
		Connect,
		Options,
		Trace,
		Invalid,
	}; };

	struct HeaderField { enum Enum {
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
		MaxForwards,			// Max-Forwards: 10
		Method,					// GET / HEAD / POST / PUT / DELETE / CONNECT / OPTIONS or TRACE
		Origin,					// Origin: http://www.example-social-network.com
		Resource,				// GET /index.html HTTP/1.1
		Pragma,					// Pragma: no-cache
		ProxyAuthorization,		// Proxy-Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
		Range,					// Range: bytes=500-999
		Referer,				// Referer: http://en.wikipedia.org/wiki/Main_Page
		UserAgent,				// User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:12.0) Gecko/20100101 Firefox/21.0
		Via						// Via: 1.0 fred, 1.1 example.com (Apache/1.1)
	}; };

	/// Functions for composing requestString
	Status		make		(Method::Enum method, const char* fullUrl);
	Status		addField	(const char* field, const char* value);
	Status		addField	(HeaderField::Enum field, const char* value);
	Status		addField	(HeaderField::Enum field, roUint64 value);
	Status		addField	(HeaderField::Enum field, roUint64 value1, roUint64 value2);

	void		removeField	(const char* field);
	void		removeField	(HeaderField::Enum field);

	Method::Enum	getMethod	() const;

	bool		getField	(const char* option, String& value) const;
	bool		getField	(const char* option, RangedString& value) const;
	bool		getField	(HeaderField::Enum option, String& value) const;
	bool		getField	(HeaderField::Enum option, RangedString& value) const;

	String string;
};	// HttpRequestHeader

struct HttpResponseHeader
{
	struct HeaderField { enum Enum {
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
		Server,						// Server: Apache/2.4.1 (Unix)
		SetCookie,					// Set-Cookie: UserID=JohnDoe; Max-Age=3600; Version=1
		Status,						// Status: 200 OK
		StrictTransportSecurity,	// Strict-Transport-Security: max-age=16070400; includeSubDomains
		Trailer,					// Trailer: Max-Forwards
		TransferEncoding,			// Transfer-Encoding: chunked
		Vary,						// Vary: *
		Version,					// HTTP/1.1 206 Partial Content
		Via,						// Via: 1.0 fred, 1.1 example.com (Apache/1.1)
		WWWAuthenticate,			// WWW-Authenticate: Basic
	}; };

	/// Parse and get information from responseString
	bool getField(const char* option, String& value) const;
	bool getField(const char* option, RangedString& value) const;
	bool getField(HeaderField::Enum option, String& value) const;
	bool getField(HeaderField::Enum option, RangedString& value) const;
	bool getField(HeaderField::Enum option, roUint64& value) const;
	bool getField(HeaderField::Enum option, roUint64& value1, roUint64& value2, roUint64& value3) const;

	String string;
};	// HttpResponseHeader

roStatus splitUrl(const RangedString& url, RangedString& protocol, RangedString& host, RangedString& path);

struct HttpOptions
{
	struct Option { enum Enum {
		MaxUrlSize,
		MaxHeaderSize,
	}; };
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

struct HttpServer
{
	HttpServer();
	~HttpServer();

	struct Connection : public ro::ListNode<HttpServer::Connection>
	{
		Connection();

		Status		update		();

		RingBuffer	ringBuf;
		BsdSocket	socket;
	};	// HttpServer::Connection

	Status	init();
	Status	update();
	Status	(*onRequest)(Connection& connection, HttpRequestHeader& request);

	LinkList<Connection> activeConnections;
	LinkList<Connection> pooledConnections;

// Private
	BsdSocket	_socketListen;
};	// HttpServer

}   // namespace ro

#endif	// __network_roHttp_h__
