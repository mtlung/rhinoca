#include "pch.h"
#include "roHttp.h"
#include "../base/roCompressedStream.h"
#include "../base/roLog.h"
#include "../base/roRegex.h"
#include "../base/roRingBuffer.h"
#include "../base/roSha1.h"
#include "../base/roTypeCast.h"
#include "../base/roUtility.h"
#include <limits.h>

namespace ro {

static DefaultAllocator _allocator;

struct HttpServerFixedSizeOStream : public OStream
{
	HttpServerFixedSizeOStream(CoSocket& socket, roSize size) : _socket(socket), _contentSize(size), _remainingSize(size) {}

	virtual	Status write(const void* buffer, roUint64 bytesToWrite) override
	{
		roUint64 toWrite = roMinOf2(bytesToWrite, _remainingSize);
		roStatus st = _socket.send(buffer, toWrite);
		if (!st) return st;
		if (toWrite != bytesToWrite)
			return roStatus::size_limit_reached;
		_remainingSize -= toWrite;
		return st;
	}

	virtual roUint64 posWrite() const override
	{
		return _contentSize - _remainingSize;
	}

	virtual Status closeWrite() override
	{
		Status st;
		if (_remainingSize == 0)
			st = roStatus::ok;
		else {
			roUint64 written = _contentSize - _remainingSize;
			roLog("warn", "HTTP Content-Length mis-match, %llu was declared but only %llu was written", _contentSize, written);
			st = roStatus::http_content_size_mismatch;
		}

		_contentSize = _remainingSize = 0;
		return st;
	}

	CoSocket& _socket;
	roUint64 _contentSize;
	roUint64 _remainingSize;
};	// HttpServerFixedSizeOStream

struct HttpServerChunkedSizeOStream : public OStream
{
	HttpServerChunkedSizeOStream(CoSocket& socket) : _socket(socket) {}

	virtual	Status write(const void* buffer, roUint64 bytesToWrite) override
	{
		if (bytesToWrite == 0)
			return roStatus::ok;

		char buf[32];
		int written = snprintf(buf, sizeof(buf), "%llx\r\n", bytesToWrite);
		roAssert(written < (int)sizeof(buf));
		if(written < 0)
			return roStatus::string_encoding_error;
		roStatus st = _socket.send(buf, written);
		if (!st) return st;
		st = _socket.send(buffer, bytesToWrite);
		if (!st) return st;

		_written += written;
		return _socket.send("\r\n", 2);
	}

	virtual roUint64 posWrite() const override
	{
		return _written;
	}

	virtual Status closeWrite() override
	{
		_written = 0;
		return _socket.send("0\r\n\r\n", 5);
	}

	CoSocket& _socket;
	roSize _written = 0;
};	// HttpServerChunkedSizeOStream

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

static roStatus responseCommon(HttpServer::Connection& connection, HttpResponseHeader& header, roSize* contentSize)
{
	header.addField(HttpResponseHeader::HeaderField::Server, "Roar");
	header.addField(HttpResponseHeader::HeaderField::Connection, connection.keepAlive ? "keep-alive" : "close");

	bool chunkedEncoding = false;
	if (contentSize)
		header.addField(HttpResponseHeader::HeaderField::ContentLength, *contentSize);
	else if (connection.supportChunkEncoding) {
		chunkedEncoding = true;
		if (connection.supportGZip)
			header.addField(HttpResponseHeader::HeaderField::ContentEncoding, "gzip");
	}
	else
		return roStatus::not_supported;

	if(chunkedEncoding)
		header.addField(HttpResponseHeader::HeaderField::TransferEncoding, "chunked");

	header.string += "\r\n";

	return connection.socket.send(header.string.c_str(), header.string.size());
}

roStatus HttpServer::Connection::response(HttpResponseHeader& header)
{
	header.addField(HttpResponseHeader::HeaderField::Server, "Roar");
	header.addField(HttpResponseHeader::HeaderField::Connection, keepAlive ? "keep-alive" : "close");
	header.addField(HttpResponseHeader::HeaderField::ContentLength, "0");
	header.string += "\r\n";
	return socket.send(header.string.c_str(), header.string.size());
}

roStatus HttpServer::Connection::response(HttpResponseHeader& header, OStream*& os)
{
	roStatus st = responseCommon(*this, header, NULL);
	if (!st) return st;

	// TODO: Resue oStream if the type was the same
	oStream = _allocator.newObj<HttpServerChunkedSizeOStream>(socket);
	if (supportGZip) {
		auto zip = _allocator.newObj<GZipOStream>();
		zip->init(std::move(oStream));
		oStream = std::move(zip);
	}

	os = oStream.ptr();
	return st;
}

roStatus HttpServer::Connection::response(HttpResponseHeader& header, OStream*& os, roSize contentSize)
{
	// Ignore contentSize if use gzip
	if (supportGZip)
		return response(header, os);

	roStatus st = responseCommon(*this, header, &contentSize);
	if (!st) return st;

	// TODO: Resue oStream if the type was the same
	oStream = _allocator.newObj<HttpServerFixedSizeOStream>(socket, contentSize);
	os = oStream.ptr();
	return st;
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
		roSize byteSize = roKB(1);
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
	httpVersion = header.getVersion();

	if (httpVersion >= HttpVersion::v1_1) {
		supportChunkEncoding = true;

		if (header.getField(HttpRequestHeader::HeaderField::Connection, rstr))
			keepAlive = rstr.cmpNoCase("close") != 0;	// keep-alive is on by default, unless "close" is specified

		if (header.getField(HttpRequestHeader::HeaderField::AcceptEncoding, rstr))
			supportGZip = (rstr.findNoCase("gzip") != RangedString::npos);
	}

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

	do {
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

		st = onRequest(c, header); if(!st) break;
		if (c.oStream.ptr()) {
			st = c.oStream->closeWrite();
			c.oStream.ref(nullptr);
			if (!st) break;
		}
	} while (c.keepAlive);

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
