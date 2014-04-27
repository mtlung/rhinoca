#include "pch.h"
#include "roSocket.h"
#include "roAtomic.h"
#include "roString.h"
#include "roStringFormat.h"
#include "roUtility.h"
#include "roTypeCast.h"
#include <stdio.h>
#include <fcntl.h>

#if roOS_WIN				// Native Windows
//#	undef FD_SETSIZE
//#	define FD_SETSIZE 10240
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	pragma comment(lib, "Ws2_32.lib")
#else								// Other POSIX OS
#	include <arpa/inet.h>
#	include <errno.h>
#	include <inttypes.h>
#	include <netdb.h>
#	include <netinet/in.h>
#	include <signal.h>
#	include <sys/socket.h>
#	include <sys/types.h>
#	include <unistd.h>
#   include <string.h>  // For memset
#endif

// A good guide on socket programming:
// http://beej.us/guide/bgnet/output/html/multipage/index.html

#ifndef SD_RECEIVE
#	define SD_RECEIVE	0x00
#	define SD_SEND		0x01
#	define SD_BOTH		0x02
#endif

#if roOS_WIN
//	typedef int			socklen_t;
//	typedef UINT_PTR	SOCKET;
//	typedef UINT_PTR	socket_t;
#endif

#ifdef OK
#	undef OK
#endif	// OK

// Unify the error code used in Linux and win32
#if roOS_WIN
#	define OK			S_OK
#else
#	define OK			0
#	define SOCKET_ERROR -1
#	define INVALID_SOCKET -1
#endif

#if defined(RHINOCA_APPLE)
#	define MSG_NOSIGNAL 0x2000	// http://lists.apple.com/archives/macnetworkprog/2002/Dec/msg00091.html
#endif

namespace ro {

static int getLastError()
{
#if roOS_WIN
	return WSAGetLastError();
#else
//	if(errno != 0)
//		perror("Network error:");
	return errno;
#endif
}

typedef BsdSocket::ErrorCode ErrorCode;

SockAddr::SockAddr()
{
	roMemZeroStruct(*_sockAddr);
	asSockAddr().sa_family = AF_INET;
	setIp(ipLoopBack());
}

SockAddr::SockAddr(const sockaddr& addr)
{
	asSockAddr() = addr;
}

SockAddr::SockAddr(roUint32 ip, roUint16 port)
{
	roMemZeroStruct(*_sockAddr);
	asSockAddr().sa_family = AF_INET;
	setIp(ip);
	setPort(port);
}

sockaddr& SockAddr::asSockAddr() const
{
	return (sockaddr&)_sockAddr;
}

struct _NetworkStarter
{
	_NetworkStarter() { BsdSocket::initApplication(); }
	~_NetworkStarter() { BsdSocket::closeApplication(); }
};

bool SockAddr::parse(const char* ip, roUint16 port)
{
	if(ip == NULL || ip[0] == 0)
		return false;

	struct addrinfo hints;
	struct addrinfo* res = NULL;

	roMemZeroStruct(hints);
	hints.ai_family = AF_INET;

	static _NetworkStarter _networkStarter;

	int myerrno = ::getaddrinfo(ip, NULL, &hints, &res);
	if (myerrno != 0) {
		errno = myerrno;
		return false;
	}

	roMemcpy(&asSockAddr().sa_data[2], &res->ai_addr->sa_data[2], 4);
	::freeaddrinfo(res);

	setPort(port);

	return true;
}

bool SockAddr::parse(const char* addressAndPort)
{
	String buf(addressAndPort);

	int port;
	if(sscanf(addressAndPort, "%[^:]:%d", buf.c_str(), &port) != 2)
		return false;

	if(!roIsValidCast(port, roUint16()))
		return false;

	parse(buf.c_str(), num_cast<roUint16>(port));
	return true;
}

roUint16 SockAddr::port() const
{
	return ntohs(*(roUint16*)asSockAddr().sa_data);
}

void SockAddr::setPort(roUint16 port)
{
	*(roUint16*)(asSockAddr().sa_data) = htons(port);
}

roUint32 SockAddr::ip() const
{
	return ntohl(*(roUint32*)(asSockAddr().sa_data + sizeof(roUint16)));
}

void SockAddr::setIp(roUint32 ip)
{
	*(roUint32*)(asSockAddr().sa_data + sizeof(roUint16)) = htonl(ip);
}

void SockAddr::asString(String& str) const
{
	str.clear();
	strFormat(str, "{}.{}.{}{}:{}",
		(roUint8)asSockAddr().sa_data[2],
		(roUint8)asSockAddr().sa_data[3],
		(roUint8)asSockAddr().sa_data[4],
		(roUint8)asSockAddr().sa_data[5],
		port()
	);
}

bool SockAddr::operator==(const SockAddr& rhs) const
{
	return ip() == rhs.ip() && port() == rhs.port();
}

bool SockAddr::operator!=(const SockAddr& rhs) const
{
	return !(*this == rhs);
}

bool SockAddr::operator<(const SockAddr& rhs) const
{
	if(ip() < rhs.ip())
		return true;
	if(ip() == rhs.ip())
		return port() < rhs.port();
	return false;
}

roUint32 SockAddr::ipLoopBack()
{
	static roUint32 ret = 0;

#if roOS_WIN
		ret = 2130706433;
#endif

	if(ret == 0) {
		SockAddr addr;
		roVerify(addr.parse("localhost", 0));
		ret = addr.ip();
	}

	return ret;
}

roUint32 SockAddr::ipAny()
{
	return 0;
}

static AtomicInteger _initCount = 0;

ErrorCode BsdSocket::initApplication()
{
	if(++_initCount > 1)
		return OK;

#if roOS_WIN
	WSADATA	wsad;

	// Note that we cannot use GetLastError to determine the error code
	// as is normally done in Windows Sockets if WSAStartup fails.
	return ::WSAStartup(WINSOCK_VERSION, &wsad);
#else
	return OK;
#endif
}

ErrorCode BsdSocket::closeApplication()
{
	if(--_initCount > 0)
		return OK;

#if roOS_WIN
	return ::WSACleanup();
#else
	return OK;
#endif
}

BsdSocket::BsdSocket()
	: lastError(0)
	, _isBlockingMode(true)
{
	roStaticAssert(sizeof(socket_t) == sizeof(_fd));
	setFd(INVALID_SOCKET);
}

BsdSocket::~BsdSocket()
{
	roVerify(close() == OK);
}

ErrorCode BsdSocket::create(SocketType type)
{
	// If this socket is not closed yet
	if(fd() != INVALID_SOCKET)
		return lastError = -1;

	switch (type) {
	case TCP:	setFd(::socket(AF_INET, SOCK_STREAM, 0));	break;
	case TCP6:	setFd(::socket(AF_INET6, SOCK_STREAM, 0));	break;
	case UDP:	setFd(::socket(AF_INET, SOCK_DGRAM, 0));	break;
	case UDP6:	setFd(::socket(AF_INET6, SOCK_DGRAM, 0));	break;
	default: return lastError = getLastError();
	}

#if !roOS_WIN
	{	// Disable SIGPIPE: http://unix.derkeiler.com/Mailing-Lists/FreeBSD/net/2007-03/msg00007.html
		// More reference: http://beej.us/guide/bgnet/output/html/multipage/sendman.html
		// http://discuss.joelonsoftware.com/default.asp?design.4.575720.7
		int b = 1;
		roVerify(setsockopt(fd(), SOL_SOCKET, SO_NOSIGPIPE, &b, sizeof(b)) == 0);
	}
#endif

	roAssert(fd() != INVALID_SOCKET);
	return lastError = OK;
}

ErrorCode BsdSocket::setBlocking(bool block)
{
#if roOS_WIN
	unsigned long a = block ? 0 : 1;
	lastError =
		ioctlsocket(fd(), FIONBIO, &a) == OK ?
		OK : getLastError();
#else
	lastError =
		fcntl(fd(), F_SETFL, fcntl(fd(), F_GETFL) | O_NONBLOCK) != -1 ?
		OK : getLastError();
#endif

	if(lastError == OK)
		_isBlockingMode = block;

	return lastError;
}

ErrorCode BsdSocket::setNoDelay(bool b)
{
	int a = b ? 1 : 0;
	return _setOption(IPPROTO_TCP, TCP_NODELAY, &a, sizeof(a));
}

ErrorCode BsdSocket::setSendTimeout(float seconds)
{
	unsigned s = unsigned(seconds);
	unsigned us = unsigned((seconds - s) * 1000 * 1000);
	timeval timeout = { s, us };
	return _setOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

ErrorCode BsdSocket::setSendBuffSize(roSize size)
{
	int n = 0;
	if(!roSafeAssign(n, size)) return -1;
	return _setOption(SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

ErrorCode BsdSocket::setReceiveBuffSize(roSize size)
{
	int n = 0;
	if(!roSafeAssign(n, size)) return -1;
	return _setOption(SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
}

ErrorCode BsdSocket::bind(const SockAddr& endPoint)
{
	sockaddr addr = endPoint.asSockAddr();
	return lastError =
		::bind(fd(), &addr, sizeof(addr)) == OK ?
		OK : getLastError();
}

ErrorCode BsdSocket::listen(unsigned backlog)
{
	return lastError =
		::listen(fd(), int(backlog)) == OK ?
		OK : getLastError();
}

ErrorCode BsdSocket::accept(BsdSocket& socket) const
{
	socket_t s;
	sockaddr addr;
	socklen_t len = sizeof(addr);

	s = ::accept(fd(), &addr, &len);
	if(s == INVALID_SOCKET)
		return lastError = getLastError();

	socket.setFd(s);
	return lastError = OK;
}

ErrorCode BsdSocket::connect(const SockAddr& endPoint)
{
	sockaddr addr = endPoint.asSockAddr();
	return lastError =
		::connect(fd(), &addr, sizeof(addr)) == OK ?
		OK : getLastError();
}

ErrorCode BsdSocket::select(bool& checkRead, bool& checkWrite, bool& checkError)
{
	fd_set readSet, writeSet, errorSet;
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_ZERO(&errorSet);
	SOCKET s = (SOCKET)fd();
	FD_SET(s, &readSet);
	FD_SET(s, &writeSet);
	FD_SET(s, &errorSet);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int ret = ::select(1,
		checkRead ? &readSet : NULL,
		checkWrite ? &writeSet : NULL,
		checkError ? &errorSet : NULL,
		&timeout
	);

	checkRead &= (FD_ISSET(s, &readSet) != 0);
	checkWrite &= (FD_ISSET(s, &writeSet) != 0);
	checkError &= (FD_ISSET(s, &errorSet) != 0);

	lastError = ret < 0 ? getLastError() : OK;

	// If check error is requested and there is really an error occurred
	if(ret > 0 && checkError) {
		socklen_t len = sizeof(lastError);
		if(getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&lastError, &len) != 0)
			lastError = getLastError();
	}

	return lastError;
}

int BsdSocket::send(const void* data, roSize len, int flags)
{
	roSize remain = len;
	roSize sent = 0;
	const char* p = (const char*)data;
	while(remain > 0) {
		int ret = ::send(fd(), p, clamp_cast<int>(remain), flags);
		if(ret < 0) {
			lastError = getLastError();
			return ret;
		}
		remain -= ret;
		sent += ret;
		p += ret;
	}
	return num_cast<int>(sent);
}

int BsdSocket::receive(void* buf, roSize len, int flags)
{
	int ret = ::recv(fd(), (char*)buf, clamp_cast<int>(len), flags);
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

int BsdSocket::sendTo(const void* data, roSize len, const SockAddr& destEndPoint, int flags)
{
	sockaddr addr = destEndPoint.asSockAddr();
	int ret = ::sendto(fd(), (const char*)data, clamp_cast<int>(len), flags, &addr, sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

int BsdSocket::receiveFrom(void* buf, roSize len, SockAddr& srcEndPoint, int flags)
{
	sockaddr& addr = srcEndPoint.asSockAddr();
	socklen_t bufSize = sizeof(addr);
	int ret = ::recvfrom(fd(), (char*)buf, clamp_cast<int>(len), flags, &addr, &bufSize);
	roAssert(bufSize == sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

ErrorCode BsdSocket::shutDownRead()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_RECEIVE) == OK)
		return lastError = OK;

#if roOS_WIN
	return lastError = getLastError();
#else
	return lastError = OK;
#endif
}

ErrorCode BsdSocket::shutDownWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_SEND) == OK)
		return lastError = OK;

#if roOS_WIN
	return lastError = getLastError();
#else
	return lastError = OK;
#endif
}

ErrorCode BsdSocket::shutDownReadWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_BOTH) == OK)
		return lastError = OK;

#if roOS_WIN
	return lastError = getLastError();
#else
	return lastError = OK;
#endif
}

ErrorCode BsdSocket::close()
{
	if(fd() == INVALID_SOCKET)
		return lastError = OK;

#if roOS_WIN
	if(::closesocket(fd()) == OK) {
		setFd(INVALID_SOCKET);
		return lastError = OK;
	}
#else
	if(::close(fd()) == OK) {
		setFd(INVALID_SOCKET);
		return lastError = OK;
	}
#endif

#if roOS_WIN
	return lastError = getLastError();
#else
	return lastError = OK;
#endif
}

bool BsdSocket::isBlockingMode() const
{
	return _isBlockingMode;
}

SockAddr BsdSocket::remoteEndPoint() const
{
	sockaddr addr;
	::memset(&addr, 0, sizeof(addr));
	socklen_t len = sizeof(addr);
	if(::getpeername(fd(), &addr, &len) == OK)
		return SockAddr(addr);
	return SockAddr(addr);
}

bool BsdSocket::inProgress(int code)
{
	return code == EINPROGRESS || code == EWOULDBLOCK;
}

ErrorCode BsdSocket::_setOption(int opt, int level, const void* p, roSize size)
{
	int n = 0;
	if(!roSafeAssign(n, size)) return -1;
	int ret = ::setsockopt(fd(), level, opt, (const char*)p, n);
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

const socket_t& BsdSocket::fd() const {
	return *reinterpret_cast<const socket_t*>(_fd);
}

void BsdSocket::setFd(const socket_t& f) {
	*reinterpret_cast<socket_t*>(_fd) = f;
}

}	// namespace ro

#include "roCoroutine.h"
#include "roLog.h"

namespace ro {

CoSocket::CoSocket()
	: _isCoBlockingMode(true)
{}

CoSocket::~CoSocket()
{
	roVerify(close() == OK);
}

struct CoSocketManager;

__declspec(thread) static CoSocketManager* _currentCoSocketManager = NULL;

struct CoSocketManager : public BgCoroutine
{
	CoSocketManager();
	~CoSocketManager();
	virtual void run() override;

	LinkList<CoSocket::Entry> socketList;
};

static DefaultAllocator _allocator;

CoSocket::ErrorCode CoSocket::create(SocketType type)
{
	_onHeap.takeOver(_allocator.newObj<OnHeap>());
	ErrorCode ret = Super::create(type);
	if(ret == OK)
		setBlocking(true);

	return ret;
}

CoSocket::ErrorCode CoSocket::setBlocking(bool block)
{
	_isCoBlockingMode = block;
	return Super::setBlocking(false);
}

CoSocket::ErrorCode CoSocket::accept(CoSocket& socket) const
{
	ErrorCode ret = Super::accept(socket);
	socket._onHeap.takeOver(_allocator.newObj<OnHeap>());

	if(!_isCoBlockingMode)
		return ret;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	if(!coroutine || !socketMgr)
		return ret;

	while(ret == WSAENOBUFS) {
		coroutine->yield();
		ret = Super::accept(socket);
	}

	if(!inProgress(ret))
		return ret;

	Entry& readEntry = _onHeap->_readEntry;

	// We pipe operation one by one
	while(readEntry.isInList())
		coroutine->yield();

	// Prepare for hand over to socket manager
	readEntry.operation = Accept;
	readEntry.fd = fd();
	readEntry.coro = coroutine;

	socketMgr->socketList.pushBack(readEntry);
	coroutine->suspend();
	roAssert(readEntry.getList() == &socketMgr->socketList);
	readEntry.removeThis();

	ret = Super::accept(socket);
	roAssert(!inProgress(ret));

	return ret;
}

CoSocket::ErrorCode CoSocket::connect(const SockAddr& endPoint, float timeOut)
{
	ErrorCode ret = Super::connect(endPoint);

	if(!_isCoBlockingMode)
		return ret;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	if(!coroutine || !socketMgr)
		return ret;

	// If we are connected many socket at once, we may end up WSAENOBUFS
	// For freshly disconnected socket, it's port number cannot be reused within some time (4 mins on windows),
	// this is called the time wait state.
	// http://www.catalyst.com/support/knowbase/100262.html
	while(ret == WSAENOBUFS) {
		coroutine->yield();
		ret = Super::connect(endPoint);
	}

	if(!inProgress(ret))
		return ret;

	Entry& readEntry = _onHeap->_readEntry;

	// We pipe operation one by one
	while(readEntry.isInList())
		coroutine->yield();

	do {
		// Prepare for hand over to socket manager
		readEntry.operation = Connect;
		readEntry.fd = fd();
		readEntry.coro = coroutine;

		socketMgr->socketList.pushBack(readEntry);
		coroutine->suspend();
		roAssert(readEntry.getList() == &socketMgr->socketList);
		readEntry.removeThis();

		// Check for connection error
		socklen_t resultLen = sizeof(ret);
		getsockopt(fd(), SOL_SOCKET, SO_ERROR, (char*)&ret, &resultLen);
	} while(timeOut > 0);

	return ret;
}

int CoSocket::send(const void* data, roSize len, int flags)
{
	if(!_isCoBlockingMode)
		return Super::send(data, len, flags);

	roSize remain = len;
	roSize sent = 0;
	const char* p = (const char*)data;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	static const roSize maxChunkSize = 1024 * 1024;

	while(remain > 0) {
		roSize chunkSize = roMinOf2(remain, maxChunkSize);
		int ret = ::send(fd(), p, clamp_cast<int>(chunkSize), flags);

		lastError = WSAGetLastError();
		if(ret <= 0) {
			if(!inProgress(lastError))
				return ret;

			Entry& writeEntry = _onHeap->_writeEntry;
			// We pipe operation one by one
			while(writeEntry.isInList())
				coroutine->yield();

			// Prepare for hand over to socket manager
			writeEntry.operation = Send;
			writeEntry.fd = fd();
			writeEntry.coro = coroutine;

			socketMgr->socketList.pushBack(writeEntry);
			coroutine->suspend();
			roAssert(writeEntry.getList() == &socketMgr->socketList);
			writeEntry.removeThis();
		}
		// In case winsock never return EINPROGRESS, we divide any huge data into smaller chunk,
		// other wise even simple memory copy will still make the function block for a long time.
		else if(remain > maxChunkSize)
			coroutine->yield();

		remain -= ret;
		sent += ret;
		p += ret;
	}

	return num_cast<int>(sent);
}

int CoSocket::receive(void* buf, roSize len, int flags)
{
	int ret = ::recv(fd(), (char*)buf, clamp_cast<int>(len), flags);

	if(!_isCoBlockingMode)
		return ret;

	if(ret > 0)
		return ret;

	lastError = WSAGetLastError();
	if(!inProgress(lastError))
		return ret;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	Entry& readEntry = _onHeap->_readEntry;

	// We pipe operation one by one
	while(readEntry.isInList())
		coroutine->yield();

	// Prepare for hand over to socket manager
	readEntry.operation = Receive;
	readEntry.fd = fd();
	readEntry.coro = coroutine;

	socketMgr->socketList.pushBack(readEntry);
	coroutine->suspend();
	roAssert(readEntry.getList() == &socketMgr->socketList);
	readEntry.removeThis();

	// Read is read
	ret = ::recv(fd(), (char*)buf, clamp_cast<int>(len), flags);
	lastError = WSAGetLastError();
	roAssert(!inProgress(lastError));
	return ret;
}

ErrorCode CoSocket::close()
{
	setBlocking(true);

	shutDownWrite();

	roByte buf[128];
	while(receive(buf, sizeof(buf)) > 0) {}

	shutDownRead();
	return Super::close();
}

bool CoSocket::isBlockingMode() const
{
	return _isCoBlockingMode;
}

BgCoroutine* createSocketManager()
{
	return new CoSocketManager;
}

CoSocketManager::CoSocketManager()
{
	roAssert(!_currentCoSocketManager);
	_currentCoSocketManager = this;
	debugName = "CoSocketManager";
}

CoSocketManager::~CoSocketManager()
{
	roAssert(_currentCoSocketManager == this);
	_currentCoSocketManager = NULL;
}

static void _select(const TinyArray<CoSocket::Entry*, FD_SETSIZE>& socketSet, fd_set& readSet, fd_set& writeSet, fd_set& errorSet)
{
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int ret = ::select((unsigned)socketSet.size(), &readSet, &writeSet, &errorSet, &timeout);
	if(ret < 0)
		roLog("error", "CoSocketManager select function fail\n");

	for(roSize i=0; i<socketSet.size(); ++i) {
		CoSocket::Entry& e = *socketSet[i];

		switch (e.operation) {
		case CoSocket::Connect:
		case CoSocket::Send:
			if(FD_ISSET(e.fd, &writeSet))
				e.coro->resume();
			break;
		case CoSocket::Accept:
		case CoSocket::Receive:
			if(FD_ISSET(e.fd, &readSet))
				e.coro->resume();
			break;
		}

		if(FD_ISSET(e.fd, &errorSet))
			e.coro->resume();
	}

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_ZERO(&errorSet);
}

void CoSocketManager::run()
{
	scheduler->_backgroundList.pushBack(bgNode);

	while(scheduler->bgKeepRun() || !socketList.isEmpty())
	{
		fd_set readSet, writeSet, errorSet;
		TinyArray<CoSocket::Entry*, FD_SETSIZE> socketSet;
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);
		FD_ZERO(&errorSet);

//		printf("socketList size: %d\n", socketList.size());

		if(socketList.isEmpty())
			suspend();

		for(CoSocket::Entry* e = socketList.begin(); e != socketList.end();)
		{
			CoSocket::Entry* next = e->next();

			FD_SET((SOCKET)e->fd, &readSet);
			FD_SET((SOCKET)e->fd, &writeSet);
			FD_SET((SOCKET)e->fd, &errorSet);
			socketSet.pushBack(e);

			if(socketSet.size() >= FD_SETSIZE) {
				_select(socketSet, readSet, writeSet, errorSet);
				socketSet.clear();
			}

			e = next;
		}

		if(!socketSet.isEmpty()) {
			_select(socketSet, readSet, writeSet, errorSet);
			socketSet.clear();
		}

		yield();
		roAssert(_currentCoSocketManager == this);
	}

	delete this;
}

}	// namespace ro
