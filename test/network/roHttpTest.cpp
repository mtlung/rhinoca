#include "pch.h"
#include "../../roar/network/roHttp.h"

using namespace ro;

struct HttpTest {};

TEST_FIXTURE(HttpTest, requestHeader)
{
	HttpRequestHeader header;

	CHECK(header.makeGet("/index.html"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Host, "localhost:8080"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Connection, "keep-alive"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::UserAgent, "Roar Game Engine"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::AcceptEncoding, "deflate"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Range, 0, 128));

	const char* expected =
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Connection: keep-alive\r\n"
		"UserAgent: Roar Game Engine\r\n"
		"Accept-Encoding: deflate\r\n"
		"Range: bytes=0-128\r\n";

	CHECK_EQUAL(expected, header.string.c_str());
}

TEST_FIXTURE(HttpTest, responseHeader)
{
	HttpResponseHeader header;
	header.string =
		"HTTP/1.1 206 Partial Content\r\n"
		"Server: nginx/1.2.6 (Ubuntu)\r\n"
		"Date: Tue, 10 Dec 2013 02:53:54 GMT\r\n"
		"Content-Type: image/png\r\n"
		"Content-Length: 65\r\n"
		"Last-Modified: Tue, 03 Dec 2013 04:40:14 GMT\r\n"
		"Connection: keep-alive\r\n"
		"Content-Range: bytes 0-64/79936\r\n";

	RangedString str;
	CHECK(header.getField(HttpResponseHeader::HeaderField::Version, str));
	CHECK_EQUAL("1.1", str.toString().c_str());

	CHECK(header.getField(HttpResponseHeader::HeaderField::Server, str));
	CHECK_EQUAL("nginx/1.2.6 (Ubuntu)", str.toString().c_str());

	CHECK(header.getField(HttpResponseHeader::HeaderField::ContentType, str));
	CHECK_EQUAL("image/png", str.toString().c_str());

	CHECK(header.getField(HttpResponseHeader::HeaderField::Connection, str));
	CHECK_EQUAL("keep-alive", str.toString().c_str());

	roUint64 val1, val2, val3;
	CHECK(header.getField(HttpResponseHeader::HeaderField::Status, val1));
	CHECK_EQUAL(206u, val1);

	CHECK(header.getField(HttpResponseHeader::HeaderField::ContentLength, val1));
	CHECK_EQUAL(65u, val1);

	CHECK(header.getField(HttpResponseHeader::HeaderField::ContentRange, val1, val2, val3));
	CHECK_EQUAL(0u, val1);
	CHECK_EQUAL(64u, val2);
	CHECK_EQUAL(79936u, val3);
}

TEST_FIXTURE(HttpTest, http)
{
	HttpRequestHeader requestHeader;
//	requestHeader.makeGet("/static-data/map3/2/0/0.png");
//	requestHeader.addField(HttpRequestHeader::HeaderField::Host, "sin-nx-lty02.ubisoft.org");
	requestHeader.makeGet("/");
	requestHeader.addField(HttpRequestHeader::HeaderField::Host, "mdc-web-tomcat17.ubisoft.org");

	HttpClient::Request request;

	HttpClient client;
	client.perform(request, requestHeader);

	Status st;
	do {
		st = request.update();
	} while(st || st == Status::in_progress);
}

TEST_FIXTURE(HttpTest, http2)
{
/*	RangedString protocol, host, path;
	HttpClient::splitUrl("http://sin-nx-lty02.ubisoft.org/static-data/map3/2/0/0.png", protocol, host, path);
	HttpClient::splitUrl("ftp://sin-nx-lty02.ubisoft.org", protocol, host, path);

	HttpClient http;
	http.debug = true;
	http.connect("sin-nx-lty02.ubisoft.org");
	http.performGet("/static-data/map3/2/0/0.png");

	Status st;
	do {
		st = http.update();
	} while(st || st == Status::in_progress);*/
}
