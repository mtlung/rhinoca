#include "pch.h"
#include "roSocket.h"
#include "roDnsResolve.h"
#include "../base/roAtomic.h"
#include "../base/roCpuProfiler.h"
#include "../base/roRegex.h"
#include "../base/roString.h"
#include "../base/roStringFormat.h"
#include "../base/roUtility.h"
#include "../base/roTypeCast.h"
#include "../platform/roCpu.h"
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

roUint16 roHtons(roUint16 hostVal)
{
	return htons(hostVal);
}

roUint16 roNtohs(roUint16 netVal)
{
	return ntohs(netVal);
}

#if roCOMPILER_VC
#	define bSwap32 _byteswap_ulong
#	define bSwap64 _byteswap_uint64
#elif roCOMPILER_GCC
#	define bSwap32 __builtin_bswap32
#	define bSwap64 __builtin_bswap64
#endif

roUint32 roHtons(roUint32 hostVal)
{
#if roCPU_LITTLE_ENDIAN
	return bSwap32(hostVal);
#else
	return hostVal;
#endif
}

roUint32 roNtohs(roUint32 netVal)
{
#if roCPU_LITTLE_ENDIAN
	return bSwap32(netVal);
#else
	return netVal;
#endif
}

roUint64 roHtons(roUint64 hostVal)
{
#if roCPU_LITTLE_ENDIAN
	return bSwap64(hostVal);
#else
	return hostVal;
#endif
}

roUint64 roNtohs(roUint64 netVal)
{
#if roCPU_LITTLE_ENDIAN
	return bSwap64(netVal);
#else
	return netVal;
#endif
}

#undef bSwap32
#undef bSwap64

namespace ro {

static int getLastError()
{
#if roOS_WIN && !defined(roMICROSOFT_ERROR_H)
	return WSAGetLastError();
#elif roOS_WIN && defined(roMICROSOFT_ERROR_H)
    switch (WSAGetLastError()) {
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
    case WSAEAFNOSUPPORT: return EAFNOSUPPORT;
    case WSAEALREADY: return EALREADY;
    case WSAECANCELLED: return ECANCELED;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAECONNRESET: return ECONNRESET;
    case WSAEDESTADDRREQ: return EDESTADDRREQ;
    case WSAEHOSTUNREACH: return EHOSTUNREACH;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEISCONN: return EISCONN;
    case WSAELOOP: return ELOOP;
    case WSAEMSGSIZE: return EMSGSIZE;
    case WSAENETDOWN: return ENETDOWN;
    case WSAENETRESET: return ENETRESET;
    case WSAENETUNREACH: return ENETUNREACH;
    case WSAENOBUFS: return ENOBUFS;
    case WSAENOPROTOOPT: return ENOPROTOOPT;
    case WSAENOTCONN: return ENOTCONN;
    case WSANO_RECOVERY: return ENOTRECOVERABLE;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEOPNOTSUPP: return EOPNOTSUPP;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAEPROTOTYPE: return EPROTOTYPE;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAEWOULDBLOCK: return EWOULDBLOCK;
    }
    roAssert("Not handled WSA error");
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
#if roOS_WIN
	case WSA_OPERATION_ABORTED:
#endif
	case ECONNABORTED:	return roStatus::net_connaborted;
	case ECONNRESET:	return roStatus::net_connreset;
	case ECONNREFUSED:	return roStatus::net_connrefused;
#if !(roOS_WIN && defined(_INC_ERRNO))
	case EHOSTDOWN:		return roStatus::net_hostdown;
#endif
	case EHOSTUNREACH:	return roStatus::net_hostunreach;
#if roOS_WIN
	case WSA_IO_PENDING:
	case WSA_IO_INCOMPLETE:
#endif
	case EINPROGRESS:	return roStatus::in_progress;
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
	case TCP:	_setFd(::socket(AF_INET,  SOCK_STREAM, 0));	break;
	case TCP6:	_setFd(::socket(AF_INET6, SOCK_STREAM, 0));	break;
	case UDP:	_setFd(::socket(AF_INET,  SOCK_DGRAM,  0));	break;
	case UDP6:	_setFd(::socket(AF_INET6, SOCK_DGRAM,  0));	break;
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
	socket._socketType = _socketType;
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
		if(ret < 0) {
			len = sent;
			return lastError = getLastError(), errorToStatus(lastError);
		}

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
	return lastError = OK, (ret == 0 ? roStatus::net_connection_close : roStatus::ok);
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

	if(fd() != INVALID_SOCKET && (_socketType == TCP || _socketType == TCP6)) {
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
	return st == roStatus::in_progress || st == roStatus::net_wouldblock;
}

bool BsdSocket::isError(roStatus st)
{
	return !st && !inProgress(st);
}

roStatus BsdSocket::_setOption(int level, int opt, const void* p, roSize size) const
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

#include "roSocket.iocp.inc"
//#include "roSocket.select.inc"