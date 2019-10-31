#include "pch.h"
#include "../../roar/network/roHttp.h"
#include "../../roar/base/roCoRoutine.h"
#include "../../roar/base/roLog.h"

using namespace ro;

struct HttpTest {};

TEST_FIXTURE(HttpTest, requestHeader)
{
	HttpRequestHeader header;

	CHECK(header.make(HttpRequestHeader::Method::Get, "/index.html"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Host, "localhost:8080"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Connection, "keep-alive"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::UserAgent, "Roar Game Engine"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::AcceptEncoding, "deflate"));
	CHECK(header.addField(HttpRequestHeader::HeaderField::Range, 0, 128));

	const char* expected =
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Connection: keep-alive\r\n"
		"User-Agent: Roar Game Engine\r\n"
		"Accept-Encoding: deflate\r\n"
		"Range: bytes=0-128\r\n";

	CHECK_EQUAL(expected, header.string.c_str());
	CHECK_EQUAL(HttpRequestHeader::Method::Get, header.getMethod());

	header.removeField("accept-Encoding");
	header.removeField(HttpRequestHeader::HeaderField::Connection);
	header.removeField("host");
	header.removeField("range");
	header.removeField("User-Agent");

	expected = "GET /index.html HTTP/1.1\r\n";
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

TEST_FIXTURE(HttpTest, server)
{
	HttpServer server;
	server.backlog = 512;
	CHECK(server.init());

/*	HttpClient client;
	HttpClient::Request request;
	HttpRequestHeader requestHeader;
	requestHeader.make(HttpRequestHeader::Method::Get, "/static-data/map3/2/0/0.png");
	requestHeader.addField(HttpRequestHeader::HeaderField::Host, "localhost");
	client.perform(request, requestHeader);

	while(true) {
		CHECK(server.update());
		CHECK(request.update());
	}*/

	server.onRequest = [&](HttpServer::Connection& connection, HttpRequestHeader& request)->roStatus
	{
		HttpRequestHeader::Method method = request.getMethod();
		if (method == HttpRequestHeader::Method::Get) {
			RangedString resourceName;
			request.getField(HttpRequestHeader::HeaderField::Resource, resourceName);
			roLog("info", "Requesting http resource: %s\n", resourceName.toString().c_str());

			HttpResponseHeader response;
			response.make(HttpResponseHeader::ResponseCode::OK);
			response.addField(HttpResponseHeader::HeaderField::Server, "Roar");
			response.addField(HttpResponseHeader::HeaderField::ContentType, "text/html; charset=utf-8");
			response.addField(HttpResponseHeader::HeaderField::Connection, connection.keepAlive ? "keep-alive" : "close");
			response.addField(HttpResponseHeader::HeaderField::ContentLength, 6);

			response.string += "\r\n";

			roSize len = response.string.size();
			connection.socket.send(response.string.c_str(), len);
			len = 6;
			connection.socket.send("Hello!", len);
		}
		else
			return Status::not_implemented;

		return Status::ok;
	};

	// A simple server echo and broadcasting text message to all connected web socket
	LinkList<HttpServer::Connection> webSocketList;
	server.onWebScoketRequest = [&](HttpServer::Connection& connection, HttpRequestHeader& request) -> roStatus
	{
		roStatus st;
		st = server.webSocketResponse(connection, request);
		if (!st) return st;

		connection.removeThis();
		webSocketList.pushBack(connection);

		while (st) {
			HttpServer::WebsocketFrame frame;
			st = server.readWebSocketFrame(connection, frame);
			if (!st) break;

			String s;
			roSize read = frame.getReamin();
			s.resize(read);
			st = frame.read(s.c_str(), read);
			if (!st) break;

			for (auto&& i = webSocketList.begin(); i != webSocketList.end(); i = i->next()) {
				st = server.writeWebSocketFrame(*i, HttpServer::WebsocketFrame::Opcode::Text, s.c_str(), s.size());
				if (!st) return st;
			}
		}
		return st;
	};

	// To use ApacheBench for benchmarking:
	// apt-get install apache2-utils
	// ab -n 1000 -c 20 http://example.com/

	auto runOne = [&]() {
		while (true)
			server.start();
	};

	for(unsigned i=server.backlog; i--;)
		coRun(runOne, "HTTP server single connection loop", 16 * 1024);
	runOne();
}

#include "../../roar/base/roIOStream.h"
#include "../../roar/base/roTypeCast.h"

roStatus onReply_noRead(const HttpResponseHeader& response, IStream& body, void* userPtr)
{
	return roStatus::ok;
}

roStatus onReply(const HttpResponseHeader& response, IStream& body, void* userPtr)
{
	roStatus st;

	roByte buf[512];
	roUint64 read = 0;
	String content;
	do {
		st = body.read(buf, sizeof(buf), read);
		content.append((char*)buf, clamp_cast<roSize>(read));
	} while(st);

	if(st == roStatus::end_of_data) {
		// Further read should keep returning roStatus::end_of_data
		st = body.read(buf, sizeof(buf), read);
		if(st == roStatus::end_of_data)
			st = roStatus::ok;
	}

	return st;
}

TEST_FIXTURE(HttpTest, client)
{
	HttpClient client;
	HttpRequestHeader header;
	CHECK(header.make(HttpRequestHeader::Method::Get, "http://google.com"));

	CHECK(client.request(header, onReply));
}

TEST_FIXTURE(HttpTest, compression)
{
	HttpClient client;
	client.logLevel = 0;
	client.useHttpCompression = true;

	HttpRequestHeader header;

	// No chunk
	CHECK(header.make(HttpRequestHeader::Method::Get, "http://www.whatsmyip.org"));
	CHECK(client.request(header, onReply));

	// With chunk
	CHECK(header.make(HttpRequestHeader::Method::Get, "http://baby-kingdom.com"));
	CHECK(client.request(header, onReply));
}

TEST_FIXTURE(HttpTest, proxy)
{
	HttpClient client;
	client.logLevel = 2;
	client.proxy = "157.7.202.27:3128";
	client.proxy = "proxy.ubisoft.io:3128";

	HttpRequestHeader header;
	CHECK(header.make(HttpRequestHeader::Method::Get, "http://google.com.sg"));
	CHECK(client.request(header, onReply_noRead));
	CHECK(client.request(header, onReply));

	client.useHttpCompression = true;
	CHECK(header.make(HttpRequestHeader::Method::Get, "http://www.whatsmyip.org"));
	CHECK(client.request(header, onReply));
}

#include "../../roar/network/roSecureSocket.h"

TEST_FIXTURE(HttpTest, https)
{
	HttpClient client;
	client.logLevel = 2;
	client.useHttpCompression = true;
	client.proxy = "proxy.ubisoft.io:3128";

	HttpRequestHeader header;
	CHECK(header.make(HttpRequestHeader::Method::Get, "https://google.com:443"));
	CHECK(client.request(header, onReply));
}
