#ifndef __roSocket_h__
#define __roSocket_h__

#include "roNonCopyable.h"
#include "../platform/roCompiler.h"
#include "../platform/roOS.h"

// Forward declaration of platform dependent type
struct sockaddr;
typedef roPtrInt socket_t;

// Unify the error code used in Linux and win32
#if roOS_WIN && !defined(_INC_ERRNO)
#	define EALREADY		WSAEALREADY		// Operation already in progress
#	define ECONNABORTED	WSAECONNABORTED	// Software caused connection abort
#	define ECONNRESET	WSAECONNRESET	// Connection reset by peer
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

///	Socket address (IPv4 + port)
struct SockAddr
{
	SockAddr();
	SockAddr(const sockaddr& addr);
	SockAddr(roUint32 ip, roUint16 port);

	bool		parse		(const char* ip, roUint16 port);
	bool		parse		(const char* ipAndPort);		///< Format: "address:port"
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
	///	Zero for no error
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
	ErrorCode	create				(SocketType type);

	ErrorCode	setBlocking			(bool block);
	ErrorCode	setNoDelay			(bool b);
	ErrorCode	setSendTimeout		(float seconds);
	ErrorCode	setSendBuffSize		(roSize size);
	ErrorCode	setReceiveBuffSize	(roSize size);

	ErrorCode	bind				(const SockAddr& endPoint);

	///	Places the socket in a listening state
	///	\param backlog Specifies the number of incoming connections that can be queued for acceptance
	ErrorCode	listen				(unsigned backlog=5);

	///	Creates a new Socket for a newly created connection.
	///	Accept extracts the first connection on the queue of pending connections on this socket.
	///	It then returns a the newly connected socket that will handle the actual connection.
	ErrorCode	accept				(BsdSocket& socket) const;

	/// Establishes a connection to a remote host.
	ErrorCode	connect				(const SockAddr& endPoint);

	int			send				(const void* data, roSize len, int flags=0);								///< Returns -1 for any error
	int			receive				(void* buf, roSize len, int flags=0);										///< Returns -1 for any error
	int			sendTo				(const void* data, roSize len, const SockAddr& destEndPoint, int flags=0);	///< Returns -1 for any error
	int			receiveFrom			(void* buf, roSize len, SockAddr& srcEndPoint, int flags=0);				///< Returns -1 for any error

	///	To assure that all data is sent and received on a connected socket before it is closed,
	///	an application should use shutDownXXX() to close connection before calling close().
	///	Reference: See MSDN on ::shutdown

	ErrorCode	shutDownRead		();
	ErrorCode	shutDownWrite		();
	ErrorCode	shutDownReadWrite	();

	void		requestClose		();
	ErrorCode	close				();

// Attributes
	/// Whether the socket is bound to a specific local port.
	bool		IsBound				() const;

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
	static bool	inProgress			(int code);

	const socket_t& fd() const;
	void setFd(const socket_t& f);

// Private
	ErrorCode	_setOption			(int level, int opt, const void* p, roSize size);
	char _fd[sizeof(void*)];	///< File descriptor
};	// BsdSocket

}	// namespace ro

#endif	// __roSocket_h__
