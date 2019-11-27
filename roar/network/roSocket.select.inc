#include "roSocket.h"
#include "../base/roCoroutine.h"
#include "../base/roCpuProfiler.h"
#include <utility>	// for std::pair

namespace ro {

CoSocket::CoSocket()
{
}

CoSocket::~CoSocket()
{
	roVerify(close());
}

roStatus CoSocket::create(SocketType type)
{
	roStatus st = Super::create(type);
	if (!st) return st;
	return Super::setBlocking(false);
}

roStatus CoSocket::setBlocking(bool block)
{
	return block ? roStatus::ok : roStatus::not_supported;
}

roStatus CoSocket::accept(CoSocket& socket) const
{
	roScopeProfile(__FUNCTION__);

	roStatus st = Super::accept(socket);
	Coroutine* coroutine = Coroutine::current();

	while(inProgress(st)) {
		auto e = std::make_pair((void*)fd(), coroutine);
		st = ioEventRead(&e);
		if (!st) return st;

		st = Super::accept(socket);
	}

	return st;
}

// TODO: Implement timeout
roStatus CoSocket::connect(const SockAddr& endPoint, float timeout)
{
	roScopeProfile(__FUNCTION__);

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
	while (blockingCoroutine)
		coroutine->yield();

	roStatus st = Super::connect(endPoint);

	Coroutine* timeOutCoroutine = NULL;
	if (inProgress(st)) {
		auto e = std::make_pair((void*)fd(), coroutine);

		if (timeout > 0.f) {
			coRun([&]() {
				timeOutCoroutine = Coroutine::current();
				if (coSleep(timeout)) {
					ioEventCancel(&e);
					coroutine->resumeWithId(ioEventWrite, (void*)roStatus::net_connaborted);
				}
				timeOutCoroutine = NULL;
				}, "Timeout sleep", roKB(4));
		}

		blockingCoroutine = coroutine;
		st = ioEventWrite(&e);
		blockingCoroutine = NULL;

		coWakeup(timeOutCoroutine);
		while (timeOutCoroutine)
			coYield();

		if (!st) return st;

		// Check for connection error
		int ret = SOCKET_ERROR;
		socklen_t resultLen = sizeof(ret);
		getsockopt(fd(), SOL_SOCKET, SO_ERROR, (char*)&ret, &resultLen);
		lastError = ret;
		st = errorToStatus(lastError);
	}

	return st;
}

roStatus CoSocket::send(const void* data, roSize len, int flags, roSize* written)
{
	roScopeProfile(__FUNCTION__);

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

	roStatus st = roStatus::ok;
	while (remain > 0) {
		roSize toSend = roMinOf2(remain, maxChunkSize);
		st = Super::send(p, toSend, flags);
		remain -= toSend;
		p += toSend;

		if (written)
			*written = len - remain;

		if (inProgress(st)) {
			auto e = std::make_pair((void*)fd(), coroutine);
			blockingCoroutine = coroutine;
			st = ioEventWrite(&e);
			blockingCoroutine = NULL;
			if (!st) return st;
			continue;
		}

		// In case winsock never return EINPROGRESS, we divide any huge data into smaller chunk,
		// other wise even simple memory copy will still make the function block for a long time.
		else if (remain > maxChunkSize)
			coroutine->yield();

		if (!st || written)
			break;
	}

	if (st && !written && remain != 0)
		st = roStatus::net_error;

	return st;
}

roStatus CoSocket::receive(void* buf, roSize& len, int flags)
{
	roScopeProfile(__FUNCTION__);

	if (len == 0)
		return roStatus::ok;
	if (!buf)
		return roStatus::pointer_is_null;

	// We pipe operation one by one
	Coroutine* coroutine = Coroutine::current();
	while (blockingCoroutine)
		coroutine->yield();

	const roSize inLen = len;
	roStatus st = Super::receive(buf, len, flags);

	while (inProgress(st)) {
		len = inLen;
		auto e = std::make_pair((void*)fd(), coroutine);
		blockingCoroutine = coroutine;
		st = ioEventRead(&e);
		blockingCoroutine = NULL;
		if (!st) return st;

		st = Super::receive(buf, len, flags);
	}

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
		if(shutDownWrite()) {
			roByte buf[128];
			roSize len;
			while (len = sizeof(buf), receive(buf, len) && len) { coroutine->yield(); }
		}
		shutDownRead();
	}

	roAssert(!blockingCoroutine);
	return Super::close();
}

}	// namespace ro
