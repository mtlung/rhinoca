#include "pch.h"
#include "roSocket.h"
#include "roDnsResolve.h"
#include "../base/roAtomic.h"
#include "../base/roRegex.h"
#include "../base/roString.h"
#include "../base/roStringFormat.h"
#include "../base/roUtility.h"
#include "../base/roTypeCast.h"
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

static roStatus errorToStatus(BsdSocket::ErrorCode errorCode)
{
	switch(errorCode) {
	case OK:			return roStatus::ok;
	case EALREADY:		return roStatus::net_already;
	case ECONNABORTED:	return roStatus::net_connaborted;
	case ECONNRESET:	return roStatus::net_connreset;
	case ECONNREFUSED:	return roStatus::net_connrefused;
	case EHOSTDOWN:		return roStatus::net_hostdown;
	case EHOSTUNREACH:	return roStatus::net_hostunreach;
	case EINPROGRESS:	return roStatus::net_inprogress;
	case ENETDOWN:		return roStatus::net_netdown;
	case ENETRESET:		return roStatus::net_netreset;
	case ENOBUFS:		return roStatus::net_nobufs;
	case ENOTCONN:		return roStatus::net_notconn;
	case ENOTSOCK:		return roStatus::net_notsock;
	case ETIMEDOUT:		return roStatus::net_timeout;
	case EWOULDBLOCK:	return roStatus::net_wouldblock;
	}
	return roStatus::net_error;
}

static roStatus noError(BsdSocket::ErrorCode& detailError)
{
	detailError = OK;
	return roStatus::ok;
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

roStatus SockAddr::parse(const char* ip, roUint16 port)
{
	if(ip == NULL || ip[0] == 0)
		return roStatus::pointer_is_null;

#if 0
	struct addrinfo hints;
	struct addrinfo* res = NULL;

	roMemZeroStruct(hints);
	hints.ai_family = AF_INET;

	static _NetworkStarter _networkStarter;

	int myerrno = ::getaddrinfo(ip, NULL, &hints, &res);
	if (myerrno != 0) {
		errno = myerrno;
		return roStatus::net_error;
	}

	roMemcpy(&asSockAddr().sa_data[2], &res->ai_addr->sa_data[2], 4);
	::freeaddrinfo(res);
#else
	roUint32 ipNum;
	roStatus st = roGetHostByName(ip, ipNum);
	if(!st) return st;

	setIp(ipNum);
#endif

	setPort(port);

	return roStatus::ok;
}

roStatus SockAddr::parse(const char* addressAndPort)
{
	String buf(addressAndPort);

	roStatus st;
	int port;
	if(sscanf(addressAndPort, "%[^:]:%d", buf.c_str(), &port) != 2)
		return st;

	st = roIsValidCast(port, roUint16());
	if(!st) return st;

	return parse(buf.c_str(), num_cast<roUint16>(port));
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
	strFormat(str, "{}.{}.{}.{}:{}",
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

roStatus SockAddr::splitHostAndPort(const RangedString& hostAndPort, RangedString& host, RangedString& port)
{
	Regex regex;
	host = "";
	port = "";

	// Reference: http://stackoverflow.com/questions/106179/regular-expression-to-match-hostname-or-ip-address
	if(!regex.match(hostAndPort, "(^(?:[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])(?:\\.(?:[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]))*)(?::([0-9]+))?$"))
		return Status::net_invalid_host_string;

	host = regex.result[1];
	port = regex.result[2];

	return Status::ok;
}

roStatus SockAddr::splitHostAndPort(const RangedString& hostAndPort, RangedString& host, roUint16& port, roUint16 defaultPort)
{
	RangedString portStr;
	port = defaultPort;

	roStatus st = splitHostAndPort(hostAndPort, host, portStr);
	if(!st) return st;

	port = roStrToUint16(portStr.toString().c_str(), defaultPort);
	return roStatus::ok;
}

static InitShutdownCounter _initCounter;

ErrorCode BsdSocket::initApplication()
{
	if(!_initCounter.tryInit())
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
	if(!_initCounter.tryShutdown())
		return OK;

#if roOS_WIN
	return ::WSACleanup();
#else
	return OK;
#endif
}

BsdSocket::BsdSocket()
	: lastError(0)
	, _socketType(Undefined)
	, _isBlockingMode(true)
{
	roStaticAssert(sizeof(socket_t) == sizeof(_fd));
	_setFd(INVALID_SOCKET);
}

BsdSocket::~BsdSocket()
{
	roVerify(close());
}

roStatus BsdSocket::create(SocketType type)
{
	// If this socket is not closed yet
	if(fd() != INVALID_SOCKET)
		return lastError = -1, errorToStatus(lastError);

	switch (type) {
	case TCP:	_setFd(::socket(AF_INET, SOCK_STREAM, 0));	break;
	case TCP6:	_setFd(::socket(AF_INET6, SOCK_STREAM, 0));	break;
	case UDP:	_setFd(::socket(AF_INET, SOCK_DGRAM, 0));	break;
	case UDP6:	_setFd(::socket(AF_INET6, SOCK_DGRAM, 0));	break;
	default: return getLastError(), errorToStatus(lastError);
	}

#if !roOS_WIN
	{	// Disable SIGPIPE: http://unix.derkeiler.com/Mailing-Lists/FreeBSD/net/2007-03/msg00007.html
		// More reference: http://beej.us/guide/bgnet/output/html/multipage/sendman.html
		// http://discuss.joelonsoftware.com/default.asp?design.4.575720.7
		int b = 1;
		roVerify(setsockopt(fd(), SOL_SOCKET, SO_NOSIGPIPE, &b, sizeof(b)) == 0);
	}
#endif

	_socketType = type;

	roAssert(fd() != INVALID_SOCKET);
	return noError(lastError);
}

roStatus BsdSocket::setBlocking(bool block)
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

	return errorToStatus(lastError);
}

roStatus BsdSocket::setNoDelay(bool b)
{
	int a = b ? 1 : 0;
	return _setOption(IPPROTO_TCP, TCP_NODELAY, &a, sizeof(a));
}

roStatus BsdSocket::setSendTimeout(float seconds)
{
	long s = long(seconds);
	long us = long((seconds - s) * 1000 * 1000);
	timeval timeout = { s, us };
	return _setOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

roStatus BsdSocket::setSendBuffSize(roSize size)
{
	int n = 0;
	roStatus st = roSafeAssign(n, size); if(!st) return st;
	return _setOption(SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

roStatus BsdSocket::setReceiveBuffSize(roSize size)
{
	int n = 0;
	roStatus st = roSafeAssign(n, size); if(!st) return st;
	return _setOption(SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
}

roStatus BsdSocket::bind(const SockAddr& endPoint)
{
	sockaddr addr = endPoint.asSockAddr();
	lastError =
		::bind(fd(), &addr, sizeof(addr)) == OK ?
		OK : getLastError();
	return errorToStatus(lastError);
}

roStatus BsdSocket::listen(unsigned backlog)
{
	lastError =
		::listen(fd(), int(backlog)) == OK ?
		OK : getLastError();
	return errorToStatus(lastError);
}

roStatus BsdSocket::accept(BsdSocket& socket) const
{
	socket_t s;
	sockaddr addr;
	socklen_t len = sizeof(addr);

	s = ::accept(fd(), &addr, &len);
	if(s == INVALID_SOCKET)
		return lastError = getLastError(), errorToStatus(lastError);

	socket.close();
	socket._setFd(s);
	return lastError = OK, roStatus::ok;
}

roStatus BsdSocket::connect(const SockAddr& endPoint)
{
	sockaddr addr = endPoint.asSockAddr();
	lastError =
		::connect(fd(), &addr, sizeof(addr)) == OK ?
		OK : getLastError();
	return errorToStatus(lastError);
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

roStatus BsdSocket::send(const void* data, roSize& len, int flags)
{
	roSize remain = len;
	roSize sent = 0;
	const char* p = (const char*)data;
	while(remain > 0) {
		int ret = ::send(fd(), p, clamp_cast<int>(remain), flags);
		if(ret < 0)
			return lastError = getLastError(), errorToStatus(lastError);

		remain -= ret;
		sent += ret;
		p += ret;
	}
	len = num_cast<int>(sent);
	return lastError = OK, roStatus::ok;
}

roStatus BsdSocket::receive(void* buf, roSize& len, int flags)
{
	int ret = ::recv(fd(), (char*)buf, clamp_cast<int>(len), flags);
	if(ret < 0) {
		len = 0;
		return lastError = getLastError(), errorToStatus(lastError);
	}

	len = num_cast<roSize>(ret);
	return lastError = OK, roStatus::ok;
}

roStatus BsdSocket::sendTo(const void* data, roSize len, const SockAddr& destEndPoint, int flags)
{
	sockaddr addr = destEndPoint.asSockAddr();
	int ret = ::sendto(fd(), (const char*)data, clamp_cast<int>(len), flags, &addr, sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	return errorToStatus(lastError);
}

roStatus BsdSocket::receiveFrom(void* buf, roSize& len, SockAddr& srcEndPoint, int flags)
{
	sockaddr& addr = srcEndPoint.asSockAddr();
	socklen_t bufSize = sizeof(addr);
	int ret = ::recvfrom(fd(), (char*)buf, clamp_cast<int>(len), flags, &addr, &bufSize);
	roAssert(bufSize == sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	len = ret < 0 ? 0 : ret;
	return errorToStatus(lastError);
}

roStatus BsdSocket::shutDownRead()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_RECEIVE) == OK)
		return lastError = OK, roStatus::ok;

#if roOS_WIN
	return lastError = getLastError(), errorToStatus(lastError);
#else
	return lastError = OK, roStatus::ok;
#endif
}

roStatus BsdSocket::shutDownWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_SEND) == OK)
		return lastError = OK, roStatus::ok;

#if roOS_WIN
	return lastError = getLastError(), errorToStatus(lastError);
#else
	return lastError = OK, roStatus::ok;
#endif
}

roStatus BsdSocket::shutDownReadWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_BOTH) == OK)
		return lastError = OK, roStatus::ok;

#if roOS_WIN
	return lastError = getLastError(), errorToStatus(lastError);
#else
	return lastError = OK, roStatus::ok;
#endif
}

roStatus BsdSocket::close()
{
	if(fd() == INVALID_SOCKET)
		return lastError = OK, roStatus::ok;

	roVerify(setBlocking(true));

	if(fd() != INVALID_SOCKET) {
		(void)shutDownWrite();
		roByte buf[128];
		roSize len;
		while(len = sizeof(buf), receive(buf, len) && len) {}
	}

#if roOS_WIN
	if(::closesocket(fd()) == OK) {
		_setFd(INVALID_SOCKET);
		return lastError = OK, roStatus::ok;
	}
#else
	if(::close(fd()) == OK) {
		_setFd(INVALID_SOCKET);
		return lastError = OK, roStatus::ok;
	}
#endif

#if roOS_WIN
	return lastError = getLastError(), errorToStatus(lastError);
#else
	return lastError = OK, roStatus::ok;
#endif
}

BsdSocket::SocketType BsdSocket::socketType() const
{
	return _socketType;
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

bool BsdSocket::inProgress(roStatus st)
{
	return st == roStatus::net_inprogress || st == roStatus::net_wouldblock;
}

bool BsdSocket::isError(roStatus st)
{
	return !st && !inProgress(st);
}

roStatus BsdSocket::_setOption(int opt, int level, const void* p, roSize size)
{
	int n = 0;
	roStatus st = roSafeAssign(n, size); if(!st) return st;
	int ret = ::setsockopt(fd(), level, opt, (const char*)p, n);
	lastError = ret < 0 ? getLastError() : OK;
	return errorToStatus(lastError);
}

const socket_t& BsdSocket::fd() const {
	return *reinterpret_cast<const socket_t*>(_fd);
}

void BsdSocket::setFd(const socket_t& f) {
	roAssert(fd() == INVALID_SOCKET && "Please close the socket before you set a new fd");
	*reinterpret_cast<socket_t*>(_fd) = f;
}

void BsdSocket::_setFd(const socket_t& f) {
	*reinterpret_cast<socket_t*>(_fd) = f;
}

}	// namespace ro

#include "../base/roCoroutine.h"
#include "../base/roLog.h"

namespace ro {

CoSocket::CoSocket()
	: _isCoBlockingMode(true)
{}

CoSocket::~CoSocket()
{
	roVerify(close());
}

struct CoSocketManager;

__declspec(thread) static CoSocketManager* _currentCoSocketManager = NULL;

struct CoSocketManager : public BgCoroutine
{
	CoSocketManager();
	~CoSocketManager();
	virtual void run() override;
	void process(CoSocket::Entry& entry);

	LinkList<CoSocket::Entry> socketList;
};

static DefaultAllocator _allocator;

roStatus CoSocket::create(SocketType type)
{
	_onHeap.takeOver(_allocator.newObj<OnHeap>());
	roStatus st = Super::create(type);
	if(!st) return st;
	return setBlocking(true);
}

roStatus CoSocket::setBlocking(bool block)
{
	_isCoBlockingMode = block;
	return Super::setBlocking(false);
}

roStatus CoSocket::accept(CoSocket& socket) const
{
	socket.close();

	roStatus st = Super::accept(socket);
	socket._onHeap.takeOver(_allocator.newObj<OnHeap>());

	if(!_isCoBlockingMode)
		return st;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	if(!coroutine || !socketMgr)
		return st;

	while(st == roStatus::net_nobufs) {
		coroutine->yield();
		st = Super::accept(socket);
	}

	if(!inProgress(st))
		return st;

	Entry& readEntry = _onHeap->_readEntry;

	// We pipe operation one by one
	while(readEntry.isInList())
		coroutine->yield();

	// Prepare for hand over to socket manager
	readEntry.operation = Accept;
	readEntry.fd = fd();
	readEntry.coro = coroutine;

	socketMgr->process(readEntry);
	roAssert(readEntry.getList() == &socketMgr->socketList);
	readEntry.removeThis();

	st = Super::accept(socket);
	roAssert(!inProgress(st));

	return st;
}

roStatus CoSocket::connect(const SockAddr& endPoint, float timeout)
{
	if(!_isCoBlockingMode) {
		if(timeout > 0.f)
			roLog("warn", "A timeout value specified for non-blocking CoSocket is ignored\n");

		return Super::connect(endPoint);
	}

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	if(!coroutine || !socketMgr)
		return Super::connect(endPoint);

	// If we are connected many socket at once, we may end up WSAENOBUFS
	// For freshly disconnected socket, it's port number cannot be reused within some time (4 mins on windows),
	// this is called the time wait state.
	// http://www.catalyst.com/support/knowbase/100262.html
	// if(ret == WSAENOBUFS) {}

	int ret = SOCKET_ERROR;
	CountDownTimer timer(timeout);
	do {
		roStatus st = Super::connect(endPoint);

		if(!inProgress(st) && timeout <= 0.f)
			return st;

		// Prepare for hand over to socket manager
		Entry& readEntry = _onHeap->_readEntry;
		readEntry.operation = Connect;
		readEntry.fd = fd();
		readEntry.coro = coroutine;

		socketMgr->process(readEntry);
		roAssert(readEntry.getList() == &socketMgr->socketList);
		readEntry.removeThis();

		// Check for connection error
		socklen_t resultLen = sizeof(ret);
		getsockopt(fd(), SOL_SOCKET, SO_ERROR, (char*)&ret, &resultLen);
	} while(!timer.isExpired() && ret != 0);

	return lastError = ret, errorToStatus(lastError);
}

roStatus CoSocket::send(const void* data, roSize& len, int flags)
{
	if(!_isCoBlockingMode)
		return Super::send(data, len, flags);

	roSize remain = len;
	roSize sent = 0;
	const char* p = (const char*)data;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	static const roSize maxChunkSize = 1024 * 1024;

	roStatus st;
	while(remain > 0) {
		roSize toSend = roMinOf2(remain, maxChunkSize);
		st = Super::send(data, toSend, flags);

		if(!st) {
			if(!inProgress(st))
				return st;

			Entry& writeEntry = _onHeap->_writeEntry;
			// We pipe operation one by one
			while(writeEntry.isInList())
				coroutine->yield();

			// Prepare for hand over to socket manager
			writeEntry.operation = Send;
			writeEntry.fd = fd();
			writeEntry.coro = coroutine;

			socketMgr->process(writeEntry);
			roAssert(writeEntry.getList() == &socketMgr->socketList);
			writeEntry.removeThis();
		}
		// In case winsock never return EINPROGRESS, we divide any huge data into smaller chunk,
		// other wise even simple memory copy will still make the function block for a long time.
		else if(remain > maxChunkSize)
			coroutine->yield();

		remain -= toSend;
		sent += toSend;
		p += toSend;
	}

	return st;
}

roStatus CoSocket::receive(void* buf, roSize& len, int flags)
{
	roSize inLen = len;
	roStatus st = Super::receive(buf, len, flags);

	if(!_isCoBlockingMode)
		return st;

	if(!inProgress(st))
		return st;

	// No data is ready for read, set back len
	len = inLen;

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

	socketMgr->process(readEntry);
	roAssert(readEntry.getList() == &socketMgr->socketList);
	readEntry.removeThis();

	// Data is now ready for read
	st = Super::receive(buf, len, flags);

	roAssert(!inProgress(st));
	return st;
}

roStatus CoSocket::sendTo(const void* data, roSize len, const SockAddr& destEndPoint, int flags)
{
	roStatus st = Super::sendTo(data, len, destEndPoint, flags);

	if(!_isCoBlockingMode)
		return st;

	if(!inProgress(st))
		return st;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	Entry& writeEntry = _onHeap->_writeEntry;

	// We pipe operation one by one
	while(writeEntry.isInList())
		coroutine->yield();

	// Prepare for hand over to socket manager
	writeEntry.operation = Send;
	writeEntry.fd = fd();
	writeEntry.coro = coroutine;

	socketMgr->process(writeEntry);
	roAssert(writeEntry.getList() == &socketMgr->socketList);
	writeEntry.removeThis();

	// Data is now ready to send
	st = Super::sendTo(data, len, destEndPoint, flags);

	roAssert(!inProgress(st));
	return st;
}

struct TimeoutTracker : public Coroutine, private NonCopyable
{
	TimeoutTracker(Coroutine* toWake, float timeoutSeconds)
		: _toWake(toWake)
	{
		_canceled = false;
		_timedOut = false;
		_timeoutSeconds = timeoutSeconds;
		debugName = "TimeOutTracker";
	}

	void cancel()
	{
		_canceled = true;
		_toWake = NULL;
	}

	bool isTimedOut() const
	{
		return _timedOut;
	}

	virtual void run() override
	{
		coSleep(_timeoutSeconds);
		_timedOut = true;
		if(_toWake)
			_toWake->resume();
		if(_canceled)
			delete this;
	}

	bool _canceled;
	bool _timedOut;
	float _timeoutSeconds;
	Coroutine* _toWake;
};	// TimeoutTracker

roStatus CoSocket::receiveFrom(void* buf, roSize& len, SockAddr& srcEndPoint, float timeout, int flags)
{
	roSize inLen = len;
	roStatus st = Super::receive(buf, len, flags);

	if(!_isCoBlockingMode)
		return st;

	if(!inProgress(st))
		return st;

	// No data is ready for read, set back len
	len = inLen;

	Coroutine* coroutine = Coroutine::current();
	CoSocketManager* socketMgr = _currentCoSocketManager;

	if(!coroutine || !socketMgr)
		return st;

	Entry& readEntry = _onHeap->_readEntry;

	// We pipe operation one by one
	while(readEntry.isInList())
		coroutine->yield();

	// Prepare for hand over to socket manager
	readEntry.operation = Receive;
	readEntry.fd = fd();
	readEntry.coro = coroutine;

	AutoPtr<TimeoutTracker> timeoutTracker;

	if(timeout > 0) {
		timeoutTracker.takeOver(_allocator.newObj<TimeoutTracker>(coroutine, timeout));
		coroutine->scheduler->add(*timeoutTracker);
	}

	socketMgr->process(readEntry);
	roAssert(readEntry.getList() == &socketMgr->socketList);
	readEntry.removeThis();

	if(timeoutTracker.ptr()) {
		if(timeoutTracker->isTimedOut())
			return roStatus::timed_out;
		else {
			timeoutTracker->cancel();
			timeoutTracker.unref();
		}
	}

	// Data is now ready for read
	st = Super::receive(buf, len, flags);

	roAssert(!inProgress(st));
	return st;
}

roStatus CoSocket::close()
{
	setBlocking(true);

	shutDownWrite();

	if(_socketType == BsdSocket::TCP || _socketType == BsdSocket::TCP6) {
		roByte buf[128];
		roSize len;
		while(len = sizeof(buf), receive(buf, len) && len) {}
	}

	shutDownRead();
	return Super::close();
}

bool CoSocket::isBlockingMode() const
{
	return _isCoBlockingMode;
}

BgCoroutine* createSocketManager()
{
	CoSocketManager* mgr = new CoSocketManager;
	mgr->initStack(1024 * 32);
	return mgr;
}

CoSocketManager::CoSocketManager()
{
	roAssert(!_currentCoSocketManager);
	_currentCoSocketManager = this;
	debugName = "CoSocketManager";
	CoSocket::initApplication();
}

CoSocketManager::~CoSocketManager()
{
	roAssert(_currentCoSocketManager == this);
	_currentCoSocketManager = NULL;
	CoSocket::closeApplication();
}

void CoSocketManager::process(CoSocket::Entry& entry)
{
	socketList.pushBack(entry);
	resume();
	roAssert(entry.coro);
	entry.coro->suspend();
}

static void _select(const TinyArray<CoSocket::Entry*, FD_SETSIZE>& socketSet, fd_set& readSet, fd_set& writeSet, fd_set& errorSet)
{
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int ret = ::select((unsigned)socketSet.size(), &readSet, &writeSet, &errorSet, &timeout);
	if(ret < 0) {
		roLog("error", "CoSocketManager select function fail\n");
		return;
	}

	bool idle = true;
	for(roSize i=0; i<socketSet.size(); ++i) {
		CoSocket::Entry& e = *socketSet[i];

		switch (e.operation) {
		case CoSocket::Connect:
		case CoSocket::Send:
			if(FD_ISSET(e.fd, &writeSet))
				e.coro->resume(NULL), idle = false;
			break;
		case CoSocket::Accept:
		case CoSocket::Receive:
			if(FD_ISSET(e.fd, &readSet))
				e.coro->resume(NULL), idle = false;
			break;
		}

		if(FD_ISSET(e.fd, &errorSet))
			e.coro->resume((void*)-1), idle = false;
	}

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_ZERO(&errorSet);

	// NOTE: We can use coSleep in a BgCoroutine :)
	if(idle)
		coSleep(1.0f / 50);
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
