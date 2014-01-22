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
	: onRequest(NULL)
{
	roVerify(BsdSocket::initApplication() == 0);
}

HttpServer::~HttpServer()
{
	roVerify(BsdSocket::closeApplication() == 0);
}

static Status _onRequest(HttpServer::Connection& connection, HttpRequestHeader& request);

Status HttpServer::init()
{
	if(_socketListen.create(BsdSocket::TCP) != 0)
		return Status::net_error;

	SockAddr anyAddr(SockAddr::ipAny(), 80);
	if(_socketListen.bind(anyAddr) != 0)
		return Status::net_error;

	if(_socketListen.listen() != 0)
		return Status::net_error;

	if(_socketListen.setBlocking(false) != 0)
		return Status::net_error;

	onRequest = _onRequest;

	return Status::ok;
}

Status HttpServer::update()
{
	{	// Check for new connection
		if(pooledConnections.isEmpty())
			pooledConnections.pushBack(*new Connection);

		Connection& c = pooledConnections.front();
		BsdSocket::ErrorCode e = _socketListen.accept(c.socket);
		if(e == 0) {
			c.removeThis();
			c.socket.setBlocking(false);
			activeConnections.pushBack(c);
		}
		else if(!BsdSocket::inProgress(e)) {
			// log error
		}
	}

	// Update each connection
	for(Connection* c = activeConnections.begin(); c != activeConnections.end(); c=c->next())
	{
		c->update();
	}

	return Status::ok;
}

HttpServer::Connection::Connection()
{

}

static Status _onRequest(HttpServer::Connection& connection, HttpRequestHeader& request)
{
	HttpRequestHeader::Method::Enum method = request.getMethod();
	switch(method) {
	case HttpRequestHeader::Method::Get:
	default:
		return Status::not_implemented;
	}

	return Status::ok;
}

Status HttpServer::Connection::update()
{
	// Read from socket
	Status st;
	roByte* wPtr = NULL;
	roSize byteSize = 1024;
	st = ringBuf.write(byteSize, wPtr);
	if(!st) return st;

	int ret = socket.receive(wPtr, byteSize);

	if(ret == 0) {
		delete this;
		return Status::ok;
	}

	ringBuf.commitWrite(ret > 0 ? ret : 0);

	// Check if header complete
	static const char headerTerminator[] = "\r\n\r\n";

	char* rPtr = (char*)ringBuf.read(byteSize);

	const char* messageContent = roStrnStr(rPtr, byteSize, headerTerminator);

	// We need to read more till we see the header terminator
	if(!messageContent)
		return Status::in_progress;

	ringBuf.commitRead(messageContent - rPtr + (sizeof(headerTerminator) - 1));

	HttpRequestHeader header;
	header.string = String(rPtr, messageContent - rPtr);

	// Get the http request header
	RangedString resourceStr;
	header.getField(HttpRequestHeader::HeaderField::Resource, resourceStr);

	HttpServer* server = roContainerof(HttpServer, activeConnections, getList());
	if(!server->onRequest)
		return Status::pointer_is_null;

	return (*server->onRequest)(*this, header);
}

}	// namespace ro
