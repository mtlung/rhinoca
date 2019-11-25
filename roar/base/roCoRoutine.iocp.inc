#include "../network/roSocket.h"
#include "roCoRoutine.iocp.inl"

namespace ro {

thread_local HANDLE ioEventCompletionPort = INVALID_HANDLE_VALUE;

roStatus ioEventInit()
{
	BsdSocket::initApplication();

	if (ioEventCompletionPort && ioEventCompletionPort != INVALID_HANDLE_VALUE)
		return roStatus::already_initialized;

	ioEventCompletionPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);
	roAssert(ioEventCompletionPort);
	return roStatus::ok;
}

roStatus ioEventRegister(void* fdCtx)
{
	HANDLE port = ::CreateIoCompletionPort(HANDLE(fdCtx), ioEventCompletionPort, NULL, 0);
	roAssert(port == ioEventCompletionPort);
	return roStatus::ok;
}

void ioEventDispatch(const bool& keepRun, const float& _timeAllowed)
{
	Coroutine* coroutine = Coroutine::current();

	while (keepRun) {
		StopWatch stopWatch;
		const float timeAllowed = roMaxOf2(0.0f, _timeAllowed);
		float t = timeAllowed;

		do {
			if (GetQueuedCompletionStatusEx != NULL) {
				OVERLAPPED_ENTRY entries[32];
				ULONG eventCount = 0;
				BOOL ret = ::GetQueuedCompletionStatusEx(ioEventCompletionPort, entries, roCountof(entries), &eventCount, int(t * 1000), FALSE);

				if (ret) for (ULONG i = 0; i < eventCount; ++i)
					static_cast<ExtendedOverlapped*>(entries[i].lpOverlapped)->coroutine->resume();

				if (!ret || eventCount == 0)
					coroutine->suspendWithId((void*)ioEventDispatch);
			}
			else {
				DWORD transferred = 0;
				ULONG_PTR completionKey = NULL;
				ExtendedOverlapped* lp = NULL;

				BOOL ret = ::GetQueuedCompletionStatus(ioEventCompletionPort, &transferred, &completionKey, reinterpret_cast<LPOVERLAPPED*>(&lp), int(t * 1000));
				roAssert(ret);
				if (lp)
					lp->coroutine->resume();
				else
					coroutine->suspendWithId((void*)ioEventDispatch);
			}

			t = roMaxOf2(timeAllowed - stopWatch.getFloat(), 0.0f);
		} while (t > 0);
	}
}

roStatus ioEventShutdown()
{
	::CloseHandle(ioEventCompletionPort);
	ioEventCompletionPort = INVALID_HANDLE_VALUE;
	BsdSocket::closeApplication();
	return roStatus::ok;
}

}	// namespace ro
