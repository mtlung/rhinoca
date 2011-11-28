#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "noncopyable.h"
#include "../roar/base/roString.h"

// Forward declaration of platform dependent type
struct sockaddr;
typedef unsigned socket_t;

// Unify the error code used in Linux and win32
#if defined(RHINOCA_WINDOWS)
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
#else
#   include <sys/errno.h>
#endif

class IPAddress
{
public:
	//!	Default constructor gives loop back ip
	IPAddress();

	explicit IPAddress(const sockaddr& ip);

	explicit IPAddress(rhuint64 ip);

// Operations
	//!	Null or empty string will result a loop back ip
	bool parse(const char* ipOrHostName);

// Attributes
	//! Provides the IP loop back address.
	static IPAddress getLoopBack();
	static IPAddress getIPv6LoopBack();

	//! Provides an IP address that indicates that the server must listen for client activity on all network interfaces.
	static IPAddress getAny();
	static IPAddress getIPv6Any();

	rhuint64 getInt() const;

	ro::ConstString getString() const;

	sockaddr& nativeAddr() const;	//! Function that cast _sockAddr into sockaddr

	bool operator==(const IPAddress& rhs) const;
	bool operator!=(const IPAddress& rhs) const;
	bool operator<(const IPAddress& rhs) const;

protected:
	char _sockAddr[16];
};	// IPAddress

//!	Represent an IP address with a port
class IPEndPoint
{
public:
	explicit IPEndPoint(const sockaddr& addr);

	//!	Default constructor gives loopback ip
	IPEndPoint(const IPAddress& address, rhuint16 port);

// Operations
	//!	String format: address:port
	bool parse(const char* addressAndPort);

// Attributes
	IPAddress& address();
	const IPAddress& address() const;
	void setAddress(const IPAddress& address);

	rhuint16 port() const;
	void setPort(rhuint16 port);

	ro::ConstString getString() const;

	bool operator==(const IPEndPoint& rhs) const;
	bool operator!=(const IPEndPoint& rhs) const;
	bool operator<(const IPEndPoint& rhs) const;

protected:
	IPAddress _address;
};	// IPEndPoint

/// Cross-platform BSD socket class
/// Reference:
/// Differences Between Windows and Unix Non-Blocking Sockets
/// http://itamarst.org/writings/win32sockets.html
class BsdSocket : Noncopyable
{
public:
	//!	Zero for no error
	typedef int ErrorCode;

	enum SocketType {
		Undefined,
		TCP,
		TCP6,
		UDP,
		UDP6
	};	// SocketType

	//! Call this before you use any function of BsdSocket
	static ErrorCode initApplication();

	//! Call this before your main exist
	static ErrorCode closeApplication();

	BsdSocket();

	~BsdSocket();

// Operations
	ErrorCode create(SocketType type);

	ErrorCode setBlocking(bool block);

	ErrorCode bind(const IPEndPoint& endPoint);

	/*!	Places the socket in a listening state
		\param backlog Specifies the number of incoming connections that can be queued for acceptance
	 */
	ErrorCode listen(unsigned backlog=5);

	/*!	Creates a new Socket for a newly created connection.
		Accept extracts the first connection on the queue of pending connections on this socket.
		It then returns a the newly connected socket that will handle the actual connection.
	 */
	ErrorCode accept(BsdSocket& socket) const;

	//! Establishes a connection to a remote host.
	ErrorCode connect(const IPEndPoint& endPoint);

	//!	Returns -1 for any error
	int send(const void* data, unsigned len, int flags=0);

	//!	Returns -1 for any error
	int receive(void* buf, unsigned len, int flags=0);

	//!	Returns -1 for any error
	int sendTo(const void* data, unsigned len, const IPEndPoint& destEndPoint, int flags=0);

	//!	Returns -1 for any error
	int receiveFrom(void* buf, unsigned len, IPEndPoint& srcEndPoint, int flags=0);

	/*	To assure that all data is sent and received on a connected socket before it is closed,
		an application should use ShutDownXXX() to close connection before calling close().
		Reference: See MSDN on ::shutdown
	 */

	/*!	Shutdown read
		Do nothing if fd() is invalid.
	 */
	ErrorCode shutDownRead();

	/*!	Shutdown write
		Do nothing if fd() is invalid.
	 */
	ErrorCode shutDownWrite();

	/*!	Shutdown read and write
		Do nothing if fd() is invalid.
	 */
	ErrorCode shutDownReadWrite();

	//! Close the socket
	void requestClose();

	//!	Do nothing if fd() is invalid.
	ErrorCode close();

// Attributes
	//! Whether the socket is bound to a specific local port.
	bool IsBound() const;

	/*!	Gets an IPEndPoint that contains the local IP address and port number to which your socket is bounded
		Throw if the socket is not bounded
	 */
	IPEndPoint boundEndPoint() const;

	/*!	If you are using a connection-oriented protocol, it gets an IPEndPoint that contains
		the remote IP address and port number to which your socket is connected to.
		If
		note/ GetRemoteEndPoint is implemented using ::getpeername
		Throw if the socket is not connected
	 */
	IPEndPoint remoteEndPoint() const;

	mutable ErrorCode lastError;

	/*! Check to see if there is system error.
		\return true if there is system error. Otherwise false.
	 */
	bool IsError();

	//!	Check the error code whether it indicating a socket operations is in progress.
	static bool inProgress(int code);

	const socket_t& fd() const;
	void setFd(const socket_t& f);

protected:
	char _fd[4];	//!< File descriptor
};	// BsdSocket

#endif	// __MCD___SOCKET_H__NETWORK_BSDSOCKET__
