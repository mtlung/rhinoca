#include "pch.h"
#include "../../roar/network/roHttp.h"

using namespace ro;

struct HttpTest {};

TEST_FIXTURE(HttpTest, responseHeader)
{
	HttpResponse response;
	response.responseString =
		"HTTP/1.1 206 Partial Content\r\n"
		"Server: nginx/1.2.6 (Ubuntu)\r\n"
		"Date: Tue, 10 Dec 2013 02:53:54 GMT\r\n"
		"Content-Type: image/png\r\n"
		"Content-Length: 65\r\n"
		"Last-Modified: Tue, 03 Dec 2013 04:40:14 GMT\r\n"
		"Connection: keep-alive\r\n"
		"Content-Range: bytes 0-64/79936\r\n";

	RangedString str;
	CHECK(response.getField(HttpResponse::HeaderField::Version, str));
	CHECK_EQUAL("1.1", str.toString().c_str());

	CHECK(response.getField(HttpResponse::HeaderField::Server, str));
	CHECK_EQUAL("nginx/1.2.6 (Ubuntu)", str.toString().c_str());

	CHECK(response.getField(HttpResponse::HeaderField::ContentType, str));
	CHECK_EQUAL("image/png", str.toString().c_str());

	CHECK(response.getField(HttpResponse::HeaderField::Connection, str));
	CHECK_EQUAL("keep-alive", str.toString().c_str());

	roUint64 val1, val2, val3;
	CHECK(response.getField(HttpResponse::HeaderField::Status, val1));
	CHECK_EQUAL(206u, val1);

	CHECK(response.getField(HttpResponse::HeaderField::ContentLength, val1));
	CHECK_EQUAL(65u, val1);

	CHECK(response.getField(HttpResponse::HeaderField::ContentRange, val1, val2, val3));
	CHECK_EQUAL(0u, val1);
	CHECK_EQUAL(64u, val2);
	CHECK_EQUAL(79936u, val3);
}
