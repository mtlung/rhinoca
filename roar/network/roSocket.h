#ifndef __network_roSocket_h__
#define __network_roSocket_h__

#include "../base/roNonCopyable.h"
#include "../base/roStatus.h"
#include "../platform/roCompiler.h"
#include "../platform/roOS.h"

// Forward declaration of platform dependent type
struct sockaddr;
typedef roPtrInt socket_t;

// Unify the error code used in Linux and win32
#if roOS_WIN && !defined(_CRT_NO_POSIX_ERROR_CODES) && defined(_INC_ERRNO)
#   define roMICROSOFT_ERROR_H
#endif

#ifndef roMICROSOFT_ERROR_H
#	define EALREADY		WSAEALREADY		// Operation already in progress
#	define ECONNABORTED	WSAECONNABORTED	// Software caused connection abort
#	define ECONNRESET	WSAECONNRESET	// Connection reset by peer
#	define ECONNREFUSED	WSAECONNREFUSED	// Connection refused
#	define EHOSTDOWN	WSAEHOSTDOWN	// Host is down
#	define EHOSTUNREACH	WSAEHOSTUNREACH	// No route to host
#	define EINPROGRESS	WSAEINPROGRESS	// Operation now in progress
#	define ENETDOWN		WSAENETDOWN		// Network is down
#	define ENETRESET	WSAENETRESET	// Network dropped connection on reset
#	define ENOBUFS		WSAENOBUFS		// No buffer space available (recoverable)
#	define ENOTCONN		WSAENOTCONN		// Socket is not connected
#	define ENOTSOCK		WSAENOTSOCK		// Socket operation on non-socket
#	define ETIMEDOUT	WSAETIMEDOUT	// Connection timed out
#	define EWOULDBLOCK	WSAEWOULDBLOCK	// Operation would block (recoverable)
#elif !roOS_WIN
#   include <sys/errno.h>
#endif

namespace ro {

struct String;
struct RangedString;

///	Socket address (IPv4 + port)
struct SockAddr
{
	SockAddr();
	SockAddr(const sockaddr& addr);
	SockAddr(roUint32 ip, roUint16 port);

	roStatus	parse		(const char* ip, roUint16 port);
	roStatus	parse		(const char* ipAndPort);		///< Format: "address:port"
	void		setIp		(roUint32 ip);
	void		setPort		(roUint16 port);

	roUint32	ip			() const;
	roUint16	port		() const;

	sockaddr&	asSockAddr	() const;

	void		asString	(String& str) const;

	bool operator==(const SockAddr& rhs) const;
	bool operator!=(const SockAddr& rhs) const;
	bool operator< (const SockAddr& rhs) const;

	static roUint32 ipLoopBack();
	static roUint32 ipAny();

	static roStatus splitHostAndPort(const RangedString& hostAndPort, RangedString& host, RangedString& port);
	static roStatus splitHostAndPort(const RangedString& hostAndPort, RangedString& host, roUint16& port, roUint16 defaultPort);

// Private
	roUint8 _sockAddr[16];
};	// SockAddr


// ----------------------------------------------------------------------

/// Cross-platform BSD socket class
/// Reference:
/// Differences Between Windows and Unix Non-Blocking Sockets
/// http://itamarst.org/writings/win32sockets.html
struct BsdSocket : NonCopyable
{
	///	Detailed error code which roStatus didn't capture. Zero for no error
	typedef int ErrorCode;

	enum SocketType {
		Undefined,
		TCP,
		TCP6,
		UDP,
		UDP6
	};	// SocketType

	/// Call this before you use any function of BsdSocket
	static ErrorCode initApplication();

	/// Call this before your main exit
	static ErrorCode closeApplication();

	BsdSocket();

	~BsdSocket();

// Operations
	roStatus	create				(SocketType type);

	roStatus	setBlocking			(bool block);
	roStatus	setNoDelay			(bool b);
	roStatus	setSendTimeout		(float seconds);
	roStatus	setSendBuffSize		(roSize size);
	roStatus	setReceiveBuffSize	(roSize size);

	roStatus	bind				(const SockAddr& endPoint);

	///	Places the socket in a listening state
	///	\param backlog Specifies the number of incoming connections that can be queued for acceptance
	roStatus	listen				(unsigned backlog=5);

	///	Creates a new Socket for a newly created connection.
	///	Accept extracts the first connection on the queue of pending connections on this socket.
	///	It then returns a the newly connected socket that will handle the actual connection.
	roStatus	accept				(BsdSocket& socket) const;

	/// Establishes a connection to a remote host.
	roStatus	connect				(const SockAddr& endPoint);

	/// A simple select function that operate only on this socket.
	ErrorCode	select				(bool& checkRead, bool& checkWrite, bool& checkError);

	roStatus	send				(const void* data, roSize& len, int flags=0);
	roStatus	receive				(void* buf, roSize& len, int flags=0);
	roStatus	sendTo				(const void* data, roSize len, const SockAddr& destEndPoint, int flags=0);
	roStatus	receiveFrom			(void* buf, roSize& len, SockAddr& srcEndPoint, int flags=0);

	///	To assure that all data is sent and received on a connected socket before it is closed,
	///	an application should use shutDownXXX() to close connection before calling close().
	///	Reference: See MSDN on ::shutdown

	roStatus	shutDownRead		();
	roStatus	shutDownWrite		();
	roStatus	shutDownReadWrite	();
	roStatus	close				();

// Attributes
	SocketType	socketType			() const;

	/// Whether the socket is bound to a specific local port.
	bool		isBound				() const;

	bool		isBlockingMode		() const;

	///	Gets an SockAddr that contains the local IP address and port number to which your socket is bounded
	///	Throw if the socket is not bounded
	SockAddr	boundEndPoint		() const;

	///	If you are using a connection-oriented protocol, it gets an SockAddr that contains
	///	the remote IP address and port number to which your socket is connected to.
	///	note/ GetRemoteEndPoint is implemented using ::getpeername
	///	Throw if the socket is not connected
	SockAddr	remoteEndPoint		() const;

	mutable ErrorCode lastError;

	///	Check to see if there is system error.
	///	\return true if there is system error. Otherwise false.
	bool		IsError				();

	///	Check the error code whether it indicating a socket operations is in progress.
	static bool	inProgress			(roStatus st);
	static bool isError				(roStatus st);

	const socket_t& fd() const;
	void setFd(const socket_t& f);

// Private
	void _setFd(const socket_t& f);

	roStatus	_setOption			(int level, int opt, const void* p, roSize size);
	SocketType	_socketType;
	char		_fd[sizeof(void*)];	///< File descriptor
	bool		_isBlockingMode;
};	// BsdSocket

}	// namespace ro

#include "../base/roLinkList.h"
#include "../base/roMemory.h"

namespace ro {

struct Coroutine;

/// Socket that make use of coroutine
struct CoSocket : public BsdSocket
{
	typedef BsdSocket Super;

	CoSocket();
	~CoSocket();

	roStatus	create			(SocketType type);
	roStatus	setBlocking		(bool block);
	roStatus	accept			(CoSocket& socket) const;
	roStatus	connect			(const SockAddr& endPoint, float timeout=0);

	roStatus	send			(const void* data, roSize& len, int flags=0);
	roStatus	receive			(void* buf, roSize& len, int flags=0);
	roStatus	sendTo			(const void* data, roSize len, const SockAddr& destEndPoint, int flags=0);
	roStatus	receiveFrom		(void* buf, roSize& len, SockAddr& srcEndPoint, float timeout=0.f, int flags=0);

	void		requestClose	();
	roStatus	close			();

	bool		isBlockingMode	() const;

// Private
	enum Operation {
		Connect,
		Accept,
		Send,
		Receive
	};
	struct OperationEntry { Operation operation; Coroutine* coro; };

	struct Entry : public ListNode<Entry>
	{
		socket_t	fd;
		Operation	operation;
		Coroutine*	coro;
	};

	// Due to how we allocate stack in Coroutine, we try to avoid CoSocketManager taking
	// pointer to CoSocket.
	struct OnHeap
	{
		Entry _readEntry;
		Entry _writeEntry;
	};

	bool					_isCoBlockingMode;
	OperationEntry			_operation;
	mutable AutoPtr<OnHeap>	_onHeap;
};	// CoSocket

}	// namespace ro

#endif	// __network_roSocket_h__
