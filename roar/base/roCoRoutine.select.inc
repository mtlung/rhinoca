#include "../network/roSocket.h"

#if roOS_WIN
#	undef FD_SETSIZE
#	define FD_SETSIZE 1024
#	include <winsock2.h>
#endif

namespace ro {

roStatus ioEventInit()
{
	BsdSocket::initApplication();
	return roStatus::ok;
}

roStatus ioEventRegister(void* platformSpecificContext)
{
	return roStatus::ok;
}

struct IoEventEntry
{
	void* fd;
	Coroutine* coroutine;
};

typedef TinyArray<const IoEventEntry*, FD_SETSIZE> IoEvents;
thread_local IoEvents* gReadEvents = NULL, * gWriteEvents = NULL;

roStatus ioEventRead(const void* ioCtx)
{
	const IoEventEntry* e = reinterpret_cast<const IoEventEntry*>(ioCtx);
	gReadEvents->pushBack(e);
	return (roStatus::Code)(roPtrInt)(e->coroutine->suspendWithId((void*)ioEventRead));
}

roStatus ioEventWrite(const void* ioCtx)
{
	const IoEventEntry* e = reinterpret_cast<const IoEventEntry*>(ioCtx);
	gWriteEvents->pushBack(e);
	return (roStatus::Code)(roPtrInt)(e->coroutine->suspendWithId((void*)ioEventWrite));
}

roStatus ioEventCancel(const void* ioCtx)
{
	gReadEvents->removeByKey(ioCtx);
	gWriteEvents->removeByKey(ioCtx);
	return roStatus::ok;
}

void ioEventDispatch(const bool& keepRun, const float& _timeAllowed)
{
	IoEvents readEvents, writeEvents;
	roAssert(!gReadEvents);
	roAssert(!gWriteEvents);
	gReadEvents = &readEvents;
	gWriteEvents = &writeEvents;

	fd_set readSet, writeSet, errorSet;
	Coroutine* coroutine = Coroutine::current();

	while (keepRun) {
		StopWatch stopWatch;
		const float timeAllowed = roMaxOf2(0.0f, _timeAllowed);
		float t = timeAllowed;

		do {
			roScopeProfile("ioEventDispatch loop");

			timeval timeout;
			timeout.tv_sec = long(t);
			timeout.tv_usec = long((t - long(t)) * 1000000);

			roAssert(readEvents.size() < FD_SETSIZE);	// TODO: Handle this limitation
			roAssert(writeEvents.size() < FD_SETSIZE);

			FD_ZERO(&readSet);
			FD_ZERO(&writeSet);
			FD_ZERO(&errorSet);
			int maxfd = 0;
			for (auto&& i : readEvents) {
				FD_SET(i->fd, &readSet);
				maxfd = roMaxOf2(maxfd, i->fd);
			}
			for (auto&& i : writeEvents) {
				FD_SET(i->fd, &writeSet);
				maxfd = roMaxOf2(maxfd, i->fd);
			}

			int ret = 0;
			// In windows select on empty sets result error
			if (!readEvents.isEmpty() || !writeEvents.isEmpty())
				ret = ::select(maxfd + 1, &readSet, &writeSet, &errorSet, &timeout);

			roAssert(ret >= 0);

			if (ret > 0) {
				for (roSize i=0; i < readEvents.size();) {
					const IoEventEntry* e = readEvents[i];
					if (FD_ISSET(e->fd, &readSet)) {
						e->coroutine->resumeWithId((void*)ioEventRead);
						readEvents.removeBySwapAt(i);
						continue;
					}
					++i;
				}
				for (roSize i=0; i < writeEvents.size();) {
					const IoEventEntry* e = writeEvents[i];
					if (FD_ISSET(e->fd, &writeSet)) {
						e->coroutine->resumeWithId((void*)ioEventWrite);
						writeEvents.removeBySwapAt(i);
						continue;
					}
					++i;
				}
			}
			else
				coroutine->suspendWithId((void*)ioEventDispatch);

			t = roMaxOf2(timeAllowed - stopWatch.getFloat(), 0.0f);
		} while (t > 0);
	}

	gReadEvents = gWriteEvents = NULL;
}

roStatus ioEventShutdown()
{
	BsdSocket::closeApplication();
	return roStatus::ok;
}

}	// namespace ro
