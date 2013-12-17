#ifndef __network_roHttp_h__
#define __network_roHttp_h__

#include "../base/roRingBuffer.h"
#include "../base/roSocket.h"
#include "../base/roString.h"

namespace ro {

struct HttpRequest
{
	struct HeaderField { enum Enum {
		Accept,					// Accept: text/plain
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
		Origin,					// Origin: http://www.example-social-network.com
		Pragma,					// Pragma: no-cache
		ProxyAuthorization,		// Proxy-Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
		Range,					// Range: bytes=500-999
		Referer,				// Referer: http://en.wikipedia.org/wiki/Main_Page
		UserAgent,				// User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:12.0) Gecko/20100101 Firefox/21.0
		Via						// Via: 1.0 fred, 1.1 example.com (Apache/1.1)
	}; };

	Status makeGet	(const char* resource);
	Status addField(const char* option, const char* value);
	Status addField(HeaderField::Enum option, const char* value);
	Status addField(HeaderField::Enum option, roUint64 value);
	Status addField(HeaderField::Enum option, roUint64 value1, roUint64 value2);

	String requestString;
};	// HttpRequest

struct HttpResponse
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

	bool getField(const char* option, RangedString& value);
	bool getField(HeaderField::Enum option, RangedString& value);
	bool getField(HeaderField::Enum option, roUint64& value);
	bool getField(HeaderField::Enum option, roUint64& value1, roUint64& value2, roUint64& value3);

	String responseString;
};	// HttpResponse

struct HttpOptions
{
	struct Option { enum Enum {
		MaxUrlSize,
		MaxHeaderSize,
	}; };
};	// HttpOptions

struct HttpClient
{
	HttpClient();
	~HttpClient();

// Operations
	Status	connect(const char* hostAndPort);
	Status	closeWrite();
	Status	closeRead();

	Status	performGet(const char* resourceName);
	Status	sendRequest(const char* request);

	Status	update();

	static Status splitUrl(const char* url, RangedString& protocol, RangedString& host, RangedString& path);

// Attributes
	template<class T>
	struct NVP
	{
		RangedString	name;
		T				value;
	};

	struct Response
	{
		void			reset();
		Status			parse(const RangedString& str);

		roUint16		statusCode;

		NVP<bool>		keepAlive;

		RangedString	location;

		RangedString	transferEncoding;
		bool			chunked, compress, deflate, gzip;

		RangedString	contentType;
		NVP<roUint64>	contentLength;

		RangedString	contentRange;
		roUint64		rangeBegin, rangeEnd, rangeTotal;

		String			string;
	};	// Response

	bool		debug;
	String		host;
	String		requestString;
	Response	response;
	RingBuffer	ringBuf;

// Private
	static const roSize _maxUrlSize = 2 * 1024;
	static const roSize _maxHeaderSize = 4 * 1024;

	Status		_funcWaitConnect();
	Status		_funcSendRequest();
	Status		_funcReadResponse();
	Status		_funcProcessResponse();
	Status		_funcReadBody();
	Status		_funcAborted();

	Status		_readFromSocket(roUint64 bytesToRead);
	Status		_parseResponseHeader();

	typedef Status (HttpClient::*NextFunc)();
	NextFunc	_nextFunc;

	BsdSocket	_socket;
	SockAddr	_sockAddr;
};	// HttpClient

struct HttpServer
{

// Private
	BsdSocket	_socket;
};	// HttpServer

}   // namespace ro

#endif	// __network_roHttp_h__
