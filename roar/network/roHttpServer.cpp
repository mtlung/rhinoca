#include "pch.h"
#include "roHttp.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roRingBuffer.h"
#include "../base/roSha1.h"
#include "../base/roTypeCast.h"
#include "../base/roUtility.h"
#include <limits.h>

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
	st = _socketListen.listen(backlog); if(!st) return st;

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
	HttpVersion httpVersion = header.getVersion();
	header.getField(HttpRequestHeader::HeaderField::Connection, rstr);
	keepAlive = (httpVersion >= HttpVersion::v1_1 && rstr.cmpNoCase("close") != 0);	// keep-alive is on by default, unless "close" is specified

	return roStatus::ok;
}

roStatus HttpServer::start()
{
	roStatus st;
	typedef HttpRequestHeader::HeaderField ReqHeaderField;
	SockAddr anyAddr(SockAddr::ipAny(), port);

	Connection c;
	st = _socketListen.accept(c.socket); if (!st) return st;

	c.removeThis();
	activeConnections.pushBack(c);

	c.keepAlive = true;
	while (c.keepAlive) {
		HttpRequestHeader header;
		st = c._processHeader(header); if (!st) break;

		// Check for connection upgrade
		// Reference:
		// https://sookocheff.com/post/networking/how-do-websockets-work/
		if (onWebScoketRequest &&
			header.cmpFieldNoCase(ReqHeaderField::Connection, "upgrade") &&
			header.cmpFieldNoCase(ReqHeaderField::Upgrade, "websocket") &&
			header.cmpFieldNoCase(ReqHeaderField::SecWebSocketVersion, "13"))
		{
			onWebScoketRequest(c, header);
			break;
		}

		st = onRequest(c, header); if (!st) break;
	}

	c.socket.close();
	return st;
}

roStatus HttpServer::webSocketResponse(Connection& connection, HttpRequestHeader& request)
{
	RangedString key;
	if (!request.getField(HttpRequestHeader::HeaderField::SecWebSocketkey, key) || key.size() == 0)
		return roStatus::http_invalid_websocket_key;

	// Calculate SHA1
	Sha1 sha1;
	unsigned char digest[20];
	sha1.update(key.begin, key.size());
	sha1.update("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");	// Web socket GUID as defined in RFC 6455
	sha1.final(digest);

	char outputDigest[64];
	if (!roBase64Encode(outputDigest, sizeof(outputDigest), &digest[0], sizeof(digest)))
		return roStatus::http_invalid_websocket_key;

	HttpResponseHeader response;
	response.make(HttpResponseHeader::ResponseCode::SwitchingProtocol);
	response.addField(HttpResponseHeader::HeaderField::Server, "Roar");
	response.addField(HttpResponseHeader::HeaderField::Upgrade, "websocket");
	response.addField(HttpResponseHeader::HeaderField::Connection, "upgrade");
	response.addField(HttpResponseHeader::HeaderField::SecWebSocketAccept, outputDigest);
	response.string += "\r\n";
	roSize len = response.string.size();
	return connection.socket.send(response.string.c_str(), len);
}

roStatus HttpServer::readWebSocketFrame(Connection& connection, WebsocketFrame& frame)
{
	unsigned char header[2];
	roSize readSize = sizeof(header);
	roStatus st = connection.socket.receive(header, readSize);
	if (!st) return st;
	if (readSize != sizeof(header))
		return roStatus::data_corrupted;

	bool finBit	= header[0] & 0x80;
	bool mask	= header[1] & 0x80;
	int opCode	= header[0] & 0x0F;
	roSize len	= header[1] & 0x7F;

	roSize lenBytes = 1;
	if (len == 126)
		lenBytes = 2;
	else if (len == 127)
		lenBytes = 8;

	if (lenBytes > 1) {
		unsigned char lenBuf[8];
		readSize = lenBytes;
		st = connection.socket.receive(lenBuf, readSize);
		if (!st) return st;
		if (readSize != lenBytes)
			return roStatus::data_corrupted;

		if (lenBytes == 2)
			len = roNtohs(*reinterpret_cast<roUint16*>(lenBuf));
		else if (lenBytes == 8)
			len = roNtohs(*reinterpret_cast<roUint64*>(lenBuf));
	}

	if (mask) {
		readSize = sizeof(frame._mask);
		st = connection.socket.receive(frame._mask, readSize);
		if (!st) return st;
		frame._maskIdx = 0;
	}
	else {
		roMemZeroStruct(frame._mask);
		// The standard require server to disconnect if client frame is not masked
		// https://tools.ietf.org/html/rfc6455#section-5.1
		return roStatus::http_unmasked_websocket_frame;
	}

	frame.isLastFrame = finBit;
	frame.opcode = WebsocketFrame::Opcode(opCode);
	frame._remain = len;
	frame._connection = &connection;

	return st;
}

roStatus HttpServer::WebsocketFrame::read(void* buf, roSize& len)
{
	len = roMinOf2(len, _remain);
	roStatus st = _connection->socket.receive(buf, len);

	if (*reinterpret_cast<roUint32*>(_mask) != 0) {
		roByte* p = reinterpret_cast<roByte*>(buf);
		for (roSize i = 0; i < len; ++i)
			p[i] ^= _mask[(_maskIdx++) & 3];
	}

	_remain -= len;
	return st;
}

roStatus HttpServer::writeWebSocketFrame(Connection& connection, WebsocketFrame::Opcode opcode, const void* data, roSize size, bool isLastFrame)
{
	roStatus st;
	unsigned char header[10] = { 0 };

	if (isLastFrame) header[0] = 0x80;
	header[0] |= (roUint8)opcode;

	roSize toSend = 0;
	if (size < 126) {
		header[1] = roUint8(size);
		toSend = 2;
	}
	else if (size < USHRT_MAX) {
		header[1] = 126;
		*reinterpret_cast<roUint16*>(header + 2) = roHtons(roUint16(size));
		toSend = 4;
	}
	else {
		header[1] = 127;
		*reinterpret_cast<roUint64*>(header + 2) = roHtons(size);
		toSend = 10;
	}

	st = connection.socket.send(header, toSend);
	if (!st) return st;

	st = connection.socket.send(data, size);
	return st;
}

}	// namespace ro
