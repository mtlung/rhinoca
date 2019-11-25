#include "roSocket.h"
#include "../base/roCoRoutine.iocp.inl"
#include <winsock2.h>
#include <mswsock.h>

#pragma comment(lib, "Mswsock.lib")

static LPFN_DISCONNECTEX DisconnectEx = NULL;
static LPFN_CONNECTEX ConnectEx = NULL;

namespace ro {

thread_local Array<SOCKET> socketPool;

CoSocket::CoSocket()
{
}

CoSocket::~CoSocket()
{
	roVerify(close());
}

roStatus CoSocket::create(SocketType type)
{
	if (DisconnectEx && !socketPool.isEmpty())
	{
		setFd(socketPool.back());
		socketPool.popBack();

		return roStatus::ok;
	}
	else
	{
		if (fd() != INVALID_SOCKET)
			return lastError = -1, errorToStatus(lastError);

		switch (type) {
		case TCP:	_setFd(::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED));	break;
		case TCP6:	_setFd(::WSASocket(AF_INET6, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED));			break;
		case UDP:	_setFd(::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED));	break;
		case UDP6:	_setFd(::WSASocket(AF_INET6, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED));				break;
		default: return getLastError(), errorToStatus(lastError);
		}
		_socketType = type;
		roAssert(fd() != INVALID_SOCKET);

		roStatus st = noError(lastError);
		if (!st) return st;

/*		int enable = 1;
		st = Super::_setOption(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
		if (!st) return st;*/

		st = ioEventRegister((void*)fd());
		if (!st) return st;

		// Prevent IO operation post event on compeletion port when the IO success right away
		// Minimum requirement: Windows Vista / Windows Server 2008
		if (!::SetFileCompletionNotificationModes((HANDLE)fd(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
			lastError = GetLastError();
			return errorToStatus(lastError);
		}

		if (type == SocketType::TCP || type == SocketType::TCP6) {
			if (false && !DisconnectEx) {	// TODO: Make socket reuse work on ConnectEx
				DWORD bytesReturned = 0;
				GUID guid = WSAID_DISCONNECTEX;
				roVerify(::WSAIoctl(fd(), SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &DisconnectEx, sizeof(DisconnectEx), &bytesReturned, NULL, NULL) == 0);
			}
		}

		return Super::setBlocking(false);
	}
}

roStatus CoSocket::setBlocking(bool block)
{
	return block ? roStatus::ok : roStatus::not_supported;
}

roStatus CoSocket::accept(CoSocket& socket) const
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
//	while (blockingCoroutine)
//		coroutine->yield();

	socket.close();
	roStatus st = socket.create(TCP);
	if (!st) return st;

	DWORD read = 0;
	ExtendedOverlapped lp;
	lp.coroutine = coroutine;
	constexpr DWORD addrSize = sizeof(sockaddr_in) + 16;
	roByte buf[2 * addrSize];

	BOOL ret = ::AcceptEx(fd(), socket.fd(), buf, 0, addrSize, addrSize, &read, &lp);
	roAssert(read == 0);

	if (!ret) {
		lastError = getLastError();
		st = errorToStatus(lastError);

		if (inProgress(st)) {
			blockingCoroutine = lp.coroutine;
			lp.coroutine->suspend();
			blockingCoroutine = NULL;
			st = roStatus::ok;
		}
		else
			return st;

		if (!::GetOverlappedResult((HANDLE)fd(), &lp, &read, FALSE))
			return roStatus::net_error;	// TODO: What should here returns
	}

	sockaddr* localAddr, *remoteAddr;
	int localAddrLen, remoteAddrLen;
	::GetAcceptExSockaddrs(buf, 0, addrSize, addrSize, &localAddr, &localAddrLen, &remoteAddr, &remoteAddrLen);

	{	// Make the accepted socket well behave
		SOCKET s = fd();
		st = socket._setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &s, sizeof(s));
		if (!st) return st;
	}

	return st;
}

// TODO: Implement timeout
roStatus CoSocket::connect(const SockAddr& endPoint, float timeout)
{
	if (!ConnectEx) {
		DWORD bytesReturned = 0;
		GUID guid = WSAID_CONNECTEX;
		roVerify(::WSAIoctl(fd(), SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx), &bytesReturned, NULL, NULL) == 0);
	}

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
//	while (blockingCoroutine)
//		coroutine->yield();

	sockaddr addr = SockAddr(SockAddr::ipAny(), 0).asSockAddr();
	lastError = ::bind(fd(), &addr, sizeof(addr)) == OK ? OK : getLastError();
	if (DisconnectEx && lastError == WSAEINVAL) lastError = OK;	// With TF_REUSE_SOCKET, the socket was already binded
	roStatus st = errorToStatus(lastError);
	if (!st) return st;

	ExtendedOverlapped lp;
	lp.coroutine = coroutine;
	addr = endPoint.asSockAddr();

	// TODO: Look at SO_UPDATE_CONNECT_CONTEXT
	BOOL ret = ::ConnectEx(fd(), &addr, sizeof(addr), NULL, 0, NULL, &lp);

	if (!ret) {
		lastError = getLastError();
		st = errorToStatus(lastError);

		if (inProgress(st)) {
/*			if (timeout > 0.f) {
				coRun([&]() {
					coSleep(timeout);
					::CancelIoEx((HANDLE)fd(), &lp);
				});
			}*/

			blockingCoroutine = lp.coroutine;
			lp.coroutine->suspend();
			blockingCoroutine = NULL;

			DWORD transferred = 0;
			lastError = ::GetOverlappedResult((HANDLE)fd(), &lp, &transferred, FALSE) ? OK : getLastError();
		}
	}

	// Make the connected socket well behave
	st = _setOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
	if (!st) return st;

	return errorToStatus(lastError);
}

roStatus CoSocket::send(const void* data, roSize len, int flags, roSize* written)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if (len == 0)
		return roStatus::ok;
	if (!data)
		return roStatus::pointer_is_null;

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
	while (blockingCoroutine)
		coroutine->yield();

	roSize remain = len;
	const roByte* p = (const roByte*)data;

	constexpr roSize maxChunkSize = roMB(1);
	ExtendedOverlapped lp;
	lp.coroutine = coroutine;

	roStatus st = roStatus::ok;
	while (remain > 0) {
		WSABUF wsab = { (ULONG)roMinOf2(remain, maxChunkSize), (CHAR*)p };

		// NOTE: Dynamic send buffer size:
		// https://www.cnblogs.com/kex1n/p/7801343.html
		int ret = ::WSASend(fd(), &wsab, 1, NULL, flags, &lp, NULL);

		if (ret != 0) {
			lastError = getLastError();
			st = errorToStatus(lastError);

			if (inProgress(st)) {
				blockingCoroutine = lp.coroutine;
				lp.coroutine->suspend();
				blockingCoroutine = NULL;
				st = roStatus::ok;
			}
			else
				return st;
		}

		DWORD sent = 0;
		if (!::GetOverlappedResult((HANDLE)fd(), &lp, &sent, FALSE))
			return roStatus::net_error;	// TODO: What should here returns

		remain -= sent;
		p += sent;
		if (written)
			*written = len - remain;

		// In case winsock never return EINPROGRESS, we divide any huge data into smaller chunk,
		// other wise even simple memory copy will still make the function block for a long time.
		else if (remain > maxChunkSize)
			coroutine->yield();

		if (written)
			break;
	}

	if (st && !written && remain != 0)
		st = roStatus::net_error;

	return st;
}

roStatus CoSocket::receive(void* buf, roSize& len, int flags)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if (len == 0)
		return roStatus::ok;
	if (!buf)
		return roStatus::pointer_is_null;

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
	while (blockingCoroutine)
		coroutine->yield();

	// NOTE: If encounter WSAENOBUFS, may consider zero bytes WSARecv
	// https://www.cnblogs.com/hnrainll/archive/2011/10/13/2210491.html

	DWORD read = 0;
	ExtendedOverlapped lp;
	lp.coroutine = coroutine;
	WSABUF wsab = { (ULONG)len, (CHAR*)buf };
	int ret = ::WSARecv(fd(), &wsab, 1, &read, (LPDWORD)&flags, &lp, NULL);
	len = read;

	roStatus st = roStatus::ok;
	if (ret != 0) {
		lastError = getLastError();
		st = errorToStatus(lastError);

		if (inProgress(st)) {
			blockingCoroutine = lp.coroutine;
			lp.coroutine->suspend();
			blockingCoroutine = NULL;
			st = roStatus::ok;
		}
		else
			return st;

		if (::GetOverlappedResult((HANDLE)fd(), &lp, &read, FALSE))
			len = read;
		else
			return roStatus::net_error;	// TODO: What should here returns
	}

	if (st && len == 0)
		return roStatus::net_connection_close;

	return st;
}

roStatus CoSocket::close()
{
	if (fd() == INVALID_SOCKET)
		return lastError = OK, roStatus::ok;

	// Wait for all operation finish
	Coroutine* coroutine = Coroutine::current();
	while (blockingCoroutine)
		coroutine->yield();

	if (_socketType == BsdSocket::TCP || _socketType == BsdSocket::TCP6) {
		if (shutDownWrite()) {
			roByte buf[128];
			roSize len;
			while (len = sizeof(buf), receive(buf, len) && len) { coroutine->yield(); }
		}
		shutDownRead();
	}

	lastError = OK;
	if (DisconnectEx && (_socketType == BsdSocket::TCP || _socketType == BsdSocket::TCP6)) {
		ExtendedOverlapped lp;
		lp.coroutine = coroutine;
		BOOL ret = ::DisconnectEx(fd(), &lp, TF_REUSE_SOCKET, 0);

		roStatus st = roStatus::ok;
		if (!ret) {
			lastError = getLastError();
			if (lastError == ENOTCONN) lastError = OK;
			st = errorToStatus(lastError);

			if (inProgress(st)) {
				blockingCoroutine = lp.coroutine;
				lp.coroutine->suspend();
				blockingCoroutine = NULL;

				DWORD transferred = 0;
				lastError = ::GetOverlappedResult((HANDLE)fd(), &lp, &transferred, FALSE) ? OK : getLastError();
			}
		}

		st = errorToStatus(lastError);
		if (st) {
			socketPool.pushBack(fd());
			_setFd(INVALID_SOCKET);
			return st;
		}
		else
			return Super::close();
	}
	else {
		if (::closesocket(fd()) == OK) {
			_setFd(INVALID_SOCKET);
			return lastError = OK, roStatus::ok;
		}

		return lastError = getLastError(), errorToStatus(lastError);
	}
}

}	// namespace ro
