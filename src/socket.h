#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "common.h"
#include "rhstring.h"

// Forward declaration of platform dependent type
struct sockaddr;
typedef unsigned socket_t;

class IPAddress
{
public:
	//!	Default constructor gives loopback ip
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

	FixString getString() const;

	sockaddr& nativeAddr() const;	//! Function that cast mSockAddr into sockaddr

	bool operator==(const IPAddress& rhs) const;
	bool operator!=(const IPAddress& rhs) const;
	bool operator<(const IPAddress& rhs) const;

protected:
	char mSockAddr[16];
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

	FixString getString() const;

	bool operator==(const IPEndPoint& rhs) const;
	bool operator!=(const IPEndPoint& rhs) const;
	bool operator<(const IPEndPoint& rhs) const;

protected:
	IPAddress mAddress;
};	// IPEndPoint

//! Cross-platform BSD socket class
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
	ErrorCode listen(size_t backlog=5);

	/*!	Creates a new Socket for a newly created connection.
		Accept extracts the first connection on the queue of pending connections on this socket.
		It then returns a the newly connected socket that will handle the actual connection.
	 */
	ErrorCode accept(BsdSocket& socket) const;

	//! Establishes a connection to a remote host.
	ErrorCode connect(const IPEndPoint& endPoint);

	//!	Returns -1 for any error
	int send(const void* data, size_t len, int flags=0);

	//!	Returns -1 for any error
	int receive(void* buf, size_t len, int flags=0);

	//!	Returns -1 for any error
	int sendTo(const void* data, size_t len, const IPEndPoint& destEndPoint, int flags=0);

	//!	Returns -1 for any error
	int receiveFrom(void* buf, size_t len, IPEndPoint& srcEndPoint, int flags=0);

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

	/*! Store the system error code to the parameter.
		\param errorCode the place the return error code will be stored in
		\return at this moment this method always returns 0.
	 */
	bool GetErrorCode(int& errorCode) const;

	//!
	bool GetSocketErrorCode(int& errorCode) const;

	//!	Check the error code whether it indicating a socket operations is in progress.
	static bool inProgress(int code);

	const socket_t& fd() const;
	void setFd(const socket_t& f);

protected:
	char mFd[4];	//!< File descriptor
	IPEndPoint mLocalEndPoint;
	mutable bool mIsConnected;
};	// BsdSocket

#endif	// __MCD___SOCKET_H__NETWORK_BSDSOCKET__
