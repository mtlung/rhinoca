#include "pch.h"
#include "roHttp.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roTypeCast.h"
#include "../base/roUtility.h"

namespace ro {

//////////////////////////////////////////////////////////////////////////
// HttpServer

HttpServer::HttpServer()
{
	roVerify(BsdSocket::initApplication() == 0);
}

HttpServer::~HttpServer()
{
	roVerify(BsdSocket::closeApplication() == 0);
}

Status HttpServer::init()
{
	roStatus st;
	SockAddr anyAddr(SockAddr::ipAny(), 80);

	st = _socketListen.create(BsdSocket::TCP); if(!st) return st;
	st = _socketListen.bind(anyAddr); if(!st) return st;
	st = _socketListen.listen(128); if(!st) return st;

	return Status::ok;
}

HttpServer::Connection::Connection()
{
}

Status HttpServer::Connection::_processHeader(HttpRequestHeader& header)
{
	// Read from socket
	Status st;
	char* rPtr = NULL;
	roByte* wPtr = NULL;
	const char* messageContent = NULL;
	static const char headerTerminator[] = "\r\n\r\n";

	RingBuffer ringBuf;
	while(!messageContent) {
		roSize byteSize = 1024;
		st = ringBuf.write(byteSize, wPtr);
		if(!st) return st;

		st = socket.receive(wPtr, byteSize);
		if (!st || byteSize == 0) return st;

		ringBuf.commitWrite(byteSize);

		// Check if header complete
		rPtr = (char*)ringBuf.read(byteSize);
		messageContent = roStrnStr(rPtr, byteSize, headerTerminator);
	}

	ringBuf.commitRead(messageContent - rPtr + (sizeof(headerTerminator) - 1));
	header.string = String(rPtr, messageContent - rPtr);

	RangedString rstr;
	HttpVersion::Enum httpVersion = header.getVersion();
	header.getField(HttpRequestHeader::HeaderField::Connection, rstr);
	keepAlive = (httpVersion >= HttpVersion::Enum::v1_1 && rstr.cmpNoCase("close") != 0);	// keep-alive is on by default, unless "close" is specified

	return roStatus::ok;
}

roStatus HttpServer::start(const OnRequest&& onRequest)
{
	roStatus st;
	SockAddr anyAddr(SockAddr::ipAny(), 80);

	Connection c;
	st = _socketListen.accept(c.socket); if (!st) return st;

	c.removeThis();
	activeConnections.pushBack(c);

	c.keepAlive = true;
	while (c.keepAlive) {
		HttpRequestHeader header;
		st = c._processHeader(header); if (!st) break;
		st = onRequest(c, header); if (!st) break;
	}

	c.socket.close();
	return st;
}

}	// namespace ro
