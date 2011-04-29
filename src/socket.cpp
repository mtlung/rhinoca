#include "Pch.h"
#include "socket.h"
#include <fcntl.h>

#if defined(RHINOCA_WINDOWS)				// Native Windows
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

#if defined(RHINOCA_WINDOWS)
//	typedef int			socklen_t;
//	typedef UINT_PTR	SOCKET;
//	typedef UINT_PTR	socket_t;
#endif

#ifdef OK
#	undef OK
#endif	// OK

// Unify the error code used in Linux and win32
#if defined(RHINOCA_WINDOWS)
#	define OK			S_OK
#else
#	define OK			0
#	define SOCKET_ERROR -1
#	define INVALID_SOCKET -1
#endif

#if defined(MCD_APPLE)
#	define MSG_NOSIGNAL 0x2000	// http://lists.apple.com/archives/macnetworkprog/2002/Dec/msg00091.html
#endif

static int getLastError()
{
#if defined(RHINOCA_WINDOWS)
	return WSAGetLastError();
#else
//	if(errno != 0)
//		perror("Network error:");
	return errno;
#endif
}

static int toInt(unsigned s) {
	return (s >= 0x7fffffff) ? 0x7fffffff : (int)s;
}

typedef BsdSocket::ErrorCode ErrorCode;

namespace {

struct BigInt
{
	union {
		rhuint64 m64;
		unsigned int mInt[2];
		unsigned short mShort[4];
		unsigned char mChar[8];
	};
};	// BigInt

}	// namespace

IPAddress::IPAddress()
{
	*this = getLoopBack();
}

IPAddress::IPAddress(rhuint64 ip)
{
	// Clear the sockaddr
	::memset(mSockAddr, 0, sizeof(sockaddr));

	// TODO: Initialize mAddress->sa_family with AF_INET6 when we support IPv6
	nativeAddr().sa_family = AF_INET;
	BigInt& bi = reinterpret_cast<BigInt&>(ip);

	// Correct the byte order first
	bi.mShort[0] = htons(bi.mShort[0]);
	bi.mShort[1] = htons(bi.mShort[1]);

	::memcpy(&nativeAddr().sa_data[2], &bi, 4);
}

IPAddress::IPAddress(const sockaddr& ip)
{
	ASSERT(AF_INET == ip.sa_family);
	ASSERT(sizeof(sockaddr) == sizeof(mSockAddr));
	nativeAddr() = ip;
}

bool IPAddress::parse(const char* ipOrHostName)
{
	if(ipOrHostName == NULL || ipOrHostName[0] == 0)
		return false;

	struct addrinfo hints;
	struct addrinfo* res = NULL;

	::memset(&hints, 0, sizeof(addrinfo));
	hints.ai_family = AF_INET;

	int myerrno = ::getaddrinfo(ipOrHostName, NULL, &hints, &res);
	if (myerrno != 0) {
		errno = myerrno;
		return false;
	}

	::memcpy(&nativeAddr().sa_data[2], &res->ai_addr->sa_data[2], 4);
	freeaddrinfo(res);

	return true;
}

IPAddress IPAddress::getLoopBack()
{
#ifdef RHINOCA_WINDOWS
	// 98048 will be encoded into 127.0.0.1
	return IPAddress(98048);
#else
	IPAddress tmp(0);
	VERIFY(tmp.parse("localhost"));
	return tmp;
#endif
}

IPAddress IPAddress::getIPv6LoopBack()
{
	ASSERT(false);
	return IPAddress(0);
}

IPAddress IPAddress::getAny() {
	return IPAddress(0);
}

IPAddress IPAddress::getIPv6Any()
{
	ASSERT(false);
	return IPAddress(0);
}

rhuint64 IPAddress::getInt() const
{
	rhuint64 ip = 0;
	BigInt& bi = reinterpret_cast<BigInt&>(ip);
	::memcpy(&bi, &nativeAddr().sa_data[2], 4);

	// Correct the byte order
	bi.mShort[0] = ntohs(bi.mShort[0]);
	bi.mShort[1] = ntohs(bi.mShort[1]);

	return ip;
}

/*std::string IPAddress::getString() const {
	std::ostringstream os;

	const sockaddr& addr = nativeAddr();
	// Note that the cast from uint8_t to unsigned is necessary
	os	<< unsigned(uint8_t(addr.sa_data[2])) << '.'
		<< unsigned(uint8_t(addr.sa_data[3])) << '.'
		<< unsigned(uint8_t(addr.sa_data[4])) << '.'
		<< unsigned(uint8_t(addr.sa_data[5]));

	return os.str();
}*/

sockaddr& IPAddress::nativeAddr() const
{
	return const_cast<sockaddr&>(
		reinterpret_cast<const sockaddr&>(mSockAddr)
	);
}

bool IPAddress::operator==(const IPAddress& rhs) const {
	return getInt() == rhs.getInt();;
}

bool IPAddress::operator!=(const IPAddress& rhs) const {
	return !(*this == rhs);
}

bool IPAddress::operator<(const IPAddress& rhs) const {
	return getInt() < rhs.getInt();;
}

IPEndPoint::IPEndPoint(const sockaddr& addr)
	: mAddress(addr)
{
}

IPEndPoint::IPEndPoint(const IPAddress& address, rhuint16 port)
	: mAddress(address)
{
	setPort(port);
}

bool IPEndPoint::parse(const char* addressAndPort)
{
	char address[64];
	char port[64];
	if(strlen(addressAndPort) >= sizeof(address)) return false;

	if(sscanf(addressAndPort, "%[^:]:%s", &address[0], &port[0]) == 2) {
		if(!mAddress.parse(address))
			return false;

		int portNum;
		if(sscanf(port, "%d", &portNum) != 1) return false;

		setPort(rhuint16(portNum));
		return true;
	}

	return mAddress.parse(address);
}

IPAddress& IPEndPoint::address() {
	return mAddress;
}

const IPAddress& IPEndPoint::address() const {
	return mAddress;
}

void IPEndPoint::setAddress(const IPAddress& address) {
	rhuint16 p = port();
	mAddress = address;
	setPort(p);
}

rhuint16 IPEndPoint::port() const {
	return rhuint16( ntohs(*(unsigned short*)(mAddress.nativeAddr().sa_data)) );
}

void IPEndPoint::setPort(rhuint16 port) {
	*(unsigned short*)(mAddress.nativeAddr().sa_data) = htons((unsigned short)port);
}

/*std::string IPEndPoint::getString() const {
	std::stringstream ss;
	ss << mAddress.getString() << ":" << port();
	return ss.str();
}*/

bool IPEndPoint::operator==(const IPEndPoint& rhs) const {
	return mAddress == rhs.mAddress && port() == rhs.port();
}

bool IPEndPoint::operator!=(const IPEndPoint& rhs) const {
	return !(*this == rhs);
}

bool IPEndPoint::operator<(const IPEndPoint& rhs) const
{
	if(mAddress < rhs.mAddress)
		return true;
	if(mAddress == rhs.mAddress)
		return port() < rhs.port();
	return false;
}

ErrorCode BsdSocket::initApplication()
{
#if defined(RHINOCA_WINDOWS)
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
#if defined(RHINOCA_WINDOWS)
	return ::WSACleanup();
#else
	return OK;
#endif
}

BsdSocket::BsdSocket()
	: lastError(0)
	, mLocalEndPoint(IPAddress::getAny(), 0)
{
	ASSERT(sizeof(socket_t) == sizeof(mFd));
	setFd(INVALID_SOCKET);
}

BsdSocket::~BsdSocket()
{
	VERIFY(close() == OK);
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

#ifndef RHINOCA_WINDOWS
	{	// Disable SIGPIPE: http://unix.derkeiler.com/Mailing-Lists/FreeBSD/net/2007-03/msg00007.html
		// More reference: http://beej.us/guide/bgnet/output/html/multipage/sendman.html
		// http://discuss.joelonsoftware.com/default.asp?design.4.575720.7
		int b = 1;
		VERIFY(setsockopt(fd(), SOL_SOCKET, SO_NOSIGPIPE, &b, sizeof(b)) == 0);
	}
#endif

	return lastError = OK;
}

ErrorCode BsdSocket::setBlocking(bool block)
{
#if defined(RHINOCA_WINDOWS)
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

ErrorCode BsdSocket::bind(const IPEndPoint& endPoint)
{
	sockaddr addr = endPoint.address().nativeAddr();
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

ErrorCode BsdSocket::connect(const IPEndPoint& endPoint)
{
	sockaddr addr = endPoint.address().nativeAddr();
	return lastError =
		::connect(fd(), &addr, sizeof(addr)) == OK ?
		OK : getLastError();
}

int BsdSocket::send(const void* data, unsigned len, int flags)
{
	unsigned remain = len;
	unsigned sent = 0;
	const char* p = (const char*)data;
	while(remain > 0) {
		int ret = ::send(fd(), p, toInt(remain), flags);
		if(ret < 0) {
			lastError = getLastError();
			return ret;
		}
		remain -= ret;
		sent += ret;
		p += ret;
	}
	return sent;
}

int BsdSocket::receive(void* buf, unsigned len, int flags)
{
	int ret = ::recv(fd(), (char*)buf, toInt(len), flags);
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

int BsdSocket::sendTo(const void* data, unsigned len, const IPEndPoint& destEndPoint, int flags)
{
	sockaddr addr = destEndPoint.address().nativeAddr();
	int ret = ::sendto(fd(), (const char*)data, toInt(len), flags, &addr, sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

int BsdSocket::receiveFrom(void* buf, unsigned len, IPEndPoint& srcEndPoint, int flags)
{
	sockaddr& addr = srcEndPoint.address().nativeAddr();
	socklen_t bufSize = sizeof(addr);
	int ret = ::recvfrom(fd(), (char*)buf, toInt(len), flags, &addr, &bufSize);
	ASSERT(bufSize == sizeof(addr));
	lastError = ret < 0 ? getLastError() : OK;
	return ret;
}

ErrorCode BsdSocket::shutDownRead()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_RECEIVE) == OK)
		return lastError = OK;

#ifdef MCD_APPLE
	return lastError = OK;
#else
	return lastError = getLastError();
#endif
}

ErrorCode BsdSocket::shutDownWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_SEND) == OK)
		return lastError = OK;

#ifdef MCD_APPLE
	return lastError = OK;
#else
	return lastError = getLastError();
#endif
}

ErrorCode BsdSocket::shutDownReadWrite()
{
	if(fd() == INVALID_SOCKET || ::shutdown(fd(), SD_BOTH) == OK)
		return lastError = OK;

#ifdef MCD_APPLE
	return lastError = OK;
#else
	return lastError = getLastError();
#endif
}

ErrorCode BsdSocket::close()
{
	if(fd() == INVALID_SOCKET)
		return lastError = OK;

#if defined(RHINOCA_WINDOWS)
	if(::closesocket(fd()) == OK)
		return lastError = OK;
#else
	if(::close(fd()) == OK)
		return lastError = OK;
#endif

#ifdef MCD_APPLE
	return lastError = OK;
#else
	return lastError = getLastError();
#endif
}

IPEndPoint BsdSocket::remoteEndPoint() const
{
	sockaddr addr;
	::memset(&addr, 0, sizeof(addr));
	socklen_t len = sizeof(addr);
	if(::getpeername(fd(), &addr, &len) == OK)
		return IPEndPoint(addr);
	return IPEndPoint(addr);
}

bool BsdSocket::inProgress(int code)
{
	return code == EINPROGRESS || code == EWOULDBLOCK;
}

const socket_t& BsdSocket::fd() const {
	return *reinterpret_cast<const socket_t*>(mFd);
}

void BsdSocket::setFd(const socket_t& f) {
	*reinterpret_cast<socket_t*>(mFd) = f;
}
