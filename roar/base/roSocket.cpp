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
#	undef FD_SETSIZE
#	define FD_SETSIZE 10240
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
	roZeroMemory(_sockAddr, sizeof(_sockAddr));
	asSockAddr().sa_family = AF_INET;
	setIp(ipLoopBack());
}

SockAddr::SockAddr(const sockaddr& addr)
{
	asSockAddr() = addr;
}

SockAddr::SockAddr(roUint32 ip, roUint16 port)
{
	roZeroMemory(_sockAddr, sizeof(_sockAddr));
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

	roZeroMemory(&hints, sizeof(addrinfo));
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

	return lastError = OK;
}

ErrorCode BsdSocket::setBlocking(bool block)
{
#if roOS_WIN
	unsigned long a = block ? 0 : 1;
	return lastError =
		ioctlsocket(fd(), FIONBIO, &a) == OK ?
		OK : getLastError();
#else
	return lastError =
		fcntl(fd(), F_SETFL, fcntl(fd(), F_GETFL) | O_NONBLOCK) != -1 ?
		OK : getLastError();
#endif
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

const socket_t& BsdSocket::fd() const {
	return *reinterpret_cast<const socket_t*>(_fd);
}

void BsdSocket::setFd(const socket_t& f) {
	*reinterpret_cast<socket_t*>(_fd) = f;
}

}	// namespace ro
