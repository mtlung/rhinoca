#include "pch.h"
#include "roMemoryProfiler.h"
#include "roAtomic.h"
#include "roSerializer.h"
#include "roStopWatch.h"
#include "roStringFormat.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "roWinDialogTemplate.h"
#include "../platform/roCpu.h"
#include "../platform/roPlatformHeaders.h"

#include <Winsock2.h>
#include <Ntsecapi.h>

#include "roFuncPatcher.h"
#include "roStackWalker.h"

#include <atomic>
#include <algorithm>

#pragma comment(lib, "DbgHelp.lib")

#define roMemoryProfilerDebug roDEBUG

namespace ro {

static RecursiveMutex	_mutex;
static DWORD			_tlsIndex = 0;
static RecursiveMutex	_callstackHashMutex;
static MemoryProfiler*	_profiler = NULL;

typedef HANDLE		(WINAPI *MyHeapCreate)(ULONG, PVOID, SIZE_T, SIZE_T, PVOID, PVOID);
typedef LPVOID		(WINAPI *MyHeapAlloc)(HANDLE, DWORD, SIZE_T);
typedef LPVOID		(WINAPI *MyHeapReAlloc)(HANDLE, DWORD, LPVOID, SIZE_T);
typedef LPVOID		(WINAPI *MyHeapFree)(HANDLE, DWORD, LPVOID);
typedef BOOL		(WINAPI *MyHeapDestroy)(HANDLE);
typedef NTSTATUS	(WINAPI *MyNtAllocateVirtualMemory)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS	(WINAPI *MyNtFreeVirtualMemory)(HANDLE, PVOID*, PSIZE_T, ULONG);

MyHeapCreate	_orgHeapCreate = NULL;
MyHeapAlloc		_orgHeapAlloc = NULL;
MyHeapReAlloc	_orgHeapReAlloc = NULL;
MyHeapFree		_orgHeapFree = NULL;
MyHeapDestroy	_orgHeapDestroy = NULL;
MyNtAllocateVirtualMemory _orgNtAllocateVirtualMemory = NULL;
MyNtFreeVirtualMemory _orgNtFreeVirtualMemory = NULL;

HANDLE		WINAPI myHeapCreate(ULONG, PVOID, SIZE_T, SIZE_T, PVOID, PVOID);
LPVOID		WINAPI myHeapAlloc(__in HANDLE, __in DWORD, __in SIZE_T);
LPVOID		WINAPI myHeapReAlloc(__in HANDLE, __in DWORD, __deref LPVOID, __in SIZE_T);
LPVOID		WINAPI myHeapFree(__in HANDLE, __in DWORD, __deref LPVOID);
BOOL		WINAPI myHeapDestroy(__in HANDLE);
NTSTATUS	WINAPI myNtAllocateVirtualMemory(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
NTSTATUS	WINAPI myNtFreeVirtualMemory(HANDLE, PVOID*, PSIZE_T, ULONG);


namespace {

// I want to use __declspec(thread) but seems it cannot be accessed when a new thread is just created.
struct TlsStruct
{
	TlsStruct() : recurseCount(0) {}
	roSize recurseCount;

	StackWalker stackWalker;

	static const roSize maxStackDepth = 512;
	void* stackBuf[maxStackDepth];
	TinyArray<roUint8, maxStackDepth * sizeof(void*)> serializeBuf;
};	// TlsStruct

// To avoid the problem of calling malloc in the malloc callback,
// this Array class use VirtualAlloc as the memory source.
template<class T>
struct Array_VirtualAlloc : public IArray<T>
{
	Array_VirtualAlloc() { this->_data = NULL; this->_capacity = 0; }
	~Array_VirtualAlloc() { this->clear(); reserve(0, true); }
	Status reserve(roSize newSize, bool force=false) override;
};	// TinyArray

template<class T>
Status Array_VirtualAlloc<T>::reserve(roSize newCapacity, bool force)
{
	if(newCapacity < _size)
		newCapacity = _size;

	T* newPtr = NULL;
	roSize roundUpSize = 0;

	if(newCapacity > 0) {
		// Round the size to nearest 4K boundary
		roundUpSize = newCapacity * sizeof(T);
		roundUpSize = (roundUpSize + 4096-1) & (roSize(-1) << 12);

		if(_capacity * sizeof(T) >= roundUpSize && !force)
			return Status::ok;

		newPtr = (T*)::VirtualAlloc(NULL, roundUpSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if(!newPtr)
			return Status::not_enough_memory;
	}

	// Move object from old memory to new memory
	if(newPtr) for(roSize i=0; i<_size; ++i)
		new (newPtr + i) T;
	if(_data && newPtr) for(roSize i=0; i<_size; ++i)
		roSwap(_data[i], newPtr[i]);
	if(_data) for(roSize i=0; i<_size; ++i)
		(_data[i]).~T();
	if(_data)
		::VirtualFree(_data, 0, MEM_RELEASE);

	_data = newPtr;
	_capacity = roundUpSize / sizeof(T);

	return Status::ok;
}

// The original TCP send function already take care of flow control,
// however each call to send() has considerable overhead.
// Therefore we make a buffer to batch multiple values into a single send()
struct FlowRegulator
{
	FlowRegulator()
	{
		roVerify(_buffer.reserve(_flushSize));
	}

	bool send(TlsStruct* tls)
	{
		ScopeRecursiveLock lock(_mutex);
		if(!_buffer.pushBack(tls->serializeBuf.bytePtr(), tls->serializeBuf.sizeInByte()))
			return false;

		tls->serializeBuf.clear();

		if(_buffer.size() > _flushSize)
			return flush();

		if(_stopWatch.getFloat() > 1)
			return flush();

		return true;
	}

	bool flush()
	{
		ScopeRecursiveLock lock(_mutex);

		roSize size = _buffer.sizeInByte();
		roSize sizeSend = size;
		_profiler->_acceptorSocket.send(_buffer.bytePtr(), sizeSend);
		if(sizeSend != size)
			return false;

		_buffer.clear();
		_stopWatch.reset();
		return true;
	}

	StopWatch _stopWatch;
	RecursiveMutex _mutex;
	Array_VirtualAlloc<char> _buffer;
	static const roSize _flushSize = 1024 * 512;
};	// FlowRegulator

// Ensure events are sent with the same order as their ticket number
static std::atomic_uint64_t _ticket = 0;
struct TicketSystem
{
	struct Info
	{
		roUint64 ticket;
		char cmd;
		void* memAddr;
		roSize memSize;
		roUint64 callstackHash;

		bool operator<(const Info& rhs) const { return ticket < rhs.ticket; }
	};

	static roUint64 getTicket()
	{
		return _ticket++;
	}

	void submit(TlsStruct* tls, roUint64 ticket, char cmd, void* memAddr, roSize memSize, roUint64 callstackHash);
	void voidTicket(TlsStruct* tls, roUint64 ticket);

	void _tryFlushTickets(TlsStruct* tls);
	void _send(TlsStruct* tls, Info& info);

	roUint64 _currentTicket = 0;
};	// TicketSystem

bool isIniting = false;

struct ResetErrorCode
{
	ResetErrorCode() : errCode(GetLastError()) {}
	~ResetErrorCode() { SetLastError(errCode); }
	void ReVerify() { if (GetLastError() != ERROR_SUCCESS) errCode = GetLastError(); }
	DWORD errCode;
};  // ResetErrorCode

}	// namespace

TlsStruct* _tlsStruct()
{
	ResetErrorCode scopedResetErrorCode;

	roAssert(_tlsIndex != 0);

	// NOTE: TlsGetValue function calls SetLastError to clear a thread's last error when it succeeds
	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));

	if(!tls) {
		if (isIniting)
			return NULL;

		isIniting = true;

		// NOTE: We never free TlsStruct. By sacrificing a little bit of memory, we gain robustness.
		// since windows' thread local suck, having no callback for clean up; while FlsAlloc do have
		// callback but FlsSetValue will allocate memory...
		tls = new TlsStruct;
		::TlsSetValue(_tlsIndex, tls);

		isIniting = false;
	}

	return tls;
}

#if roMemoryProfilerDebug

struct AddrHeapPair
{
	const roByte* addr;
	SIZE_T size;
	const roByte* end() const { return addr + size; }
	bool operator<(const AddrHeapPair& rhs) const { return addr < rhs.addr; }
	bool operator==(const AddrHeapPair& rhs) const { return addr == rhs.addr; }
};

static Array<AddrHeapPair> _debug;
void debugCheck(char cmd, const roByte* addr, SIZE_T size)
{
	if(cmd == 'a')
	{
		if(std::binary_search(_debug.begin(), _debug.end(), AddrHeapPair{ addr, size })) {
			auto&& it = std::find(_debug.begin(), _debug.end(), AddrHeapPair{ addr, size });
			(void)it;
			_roDebugBreak();
		}

		_debug.insertSorted({ addr, size });
	}
	else if(cmd == 'f')
	{
		auto&& it = std::lower_bound(_debug.begin(), _debug.end(), AddrHeapPair{ addr, 0 });
		if(it != _debug.end() && it->addr == addr)
			_debug.removeAt(it - _debug.begin());
		else
			_roDebugBreak();
	}
	if(cmd == 'V') {
		// Find overlapped regions
		size_t it1 = std::upper_bound(_debug.begin(), _debug.end(), AddrHeapPair{ addr, 0 }) - 1 - _debug.begin();
		size_t it2 = std::lower_bound(_debug.begin(), _debug.end(), AddrHeapPair{ addr + size, 0 }) - _debug.begin();

		for(size_t it=it1; it < it2; ++it) {
			const roByte* p1 = roMaxOf2(addr, _debug[it].end());
			const roByte* p2 = roMinOf2(addr + size, _debug[it + 1].addr);
			if(SIZE_T newSize = p2 - p1) {
				_debug.insertSorted({ p1, newSize });
				++it; ++it1; ++it2;
			}
		}
	}
	else if(cmd == 'v') {
		size_t it1 = std::upper_bound(_debug.begin(), _debug.end(), AddrHeapPair{ addr, 0 }) - 1 - _debug.begin();
		size_t it2 = std::lower_bound(_debug.begin(), _debug.end(), AddrHeapPair{ addr + size, 0 }) - _debug.begin();

		for(size_t it=it1; it < it2; ++it) {
			if(addr <= _debug[it].addr && _debug[it].end() <= (addr + size)) {
				_debug.removeAt(it);
				--it; --it1; --it2;
			}
			else {
				if(it == it1)
					_debug[it].size = roMinOf2(_debug[it].end(), addr) - _debug[it].addr;
				else if(it + 1 == it2) {
					const roByte* newAddr = roMaxOf2(_debug[it].addr, addr + size);
					_debug[it].size -= newAddr - _debug[it].addr;
					_debug[it].addr = newAddr;
				}
				else {
					roAssert(false);
				}
			}
		}
	}
}

#endif	// roMemoryProfilerDebug

MemoryProfiler::MemoryProfiler()
{
}

MemoryProfiler::~MemoryProfiler()
{
	shutdown();
}

static LRESULT CALLBACK dlgProc(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch(wParam) {
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

// NOTE: The order of these static variable is important!
static InitShutdownCounter _initCounter;
static FlowRegulator _regulator;
static TicketSystem _ticketSystem;
static FunctionPatcher	_functionPatcher;
static Array_VirtualAlloc<roUint64> _callstackHash;
static Array_VirtualAlloc<TicketSystem::Info> _ticketBuffer;

void TicketSystem::submit(TlsStruct* tls, roUint64 ticket, char cmd, void* memAddr, roSize memSize, roUint64 callstackHash)
{
	Info info = { ticket, cmd, memAddr, memSize, callstackHash };

	ScopeRecursiveLock lock(_callstackHashMutex);

	// Out of order submission, save it for later.
	if(ticket == _currentTicket) {
		++_currentTicket;
		_send(tls, info);
	}
	else
		_ticketBuffer.insertSorted(info);

	_tryFlushTickets(tls);
}

void TicketSystem::voidTicket(TlsStruct* tls, roUint64 ticket)
{
	ScopeRecursiveLock lock(_callstackHashMutex);
	if(ticket == _currentTicket)
		++_currentTicket;
	else
		_ticketBuffer.insertSorted({ ticket, '\0', NULL, 0, 0 });

	_tryFlushTickets(tls);
}

void TicketSystem::_tryFlushTickets(TlsStruct* tls)
{
	while(!_ticketBuffer.isEmpty() && _ticketBuffer[0].ticket == _currentTicket) {
		++_currentTicket;

		if(_ticketBuffer[0].cmd != '\0')	// Skip void ticket
			_send(tls, _ticketBuffer[0]);

		_ticketBuffer.removeAt(0);
	}
}

void TicketSystem::_send(TlsStruct* tls, Info& info)
{
	Serializer s;
	s.setBuf(&tls->serializeBuf);

	s.io(info.cmd);
	roUint64 memAddr64 = (roUint64)info.memAddr;
	s.ioVary(memAddr64);
	s.ioVary(info.memSize);

	if(info.cmd == 'a' || info.cmd == 'V')
		s.ioVary(info.callstackHash);

	_regulator.send(tls);
	_regulator.flush();

#if roMemoryProfilerDebug
	debugCheck(info.cmd, (roByte*)info.memAddr, info.memSize);
#endif	// roMemoryProfilerDebug
}

Status MemoryProfiler::init(roUint16 listeningPort)
{
	if(listeningPort == 0)
		return roStatus::ok;

	if(!_initCounter.tryInit())
		return roStatus::ok;

#if roMemoryProfilerDebug
	_debug.reserve(1024 * 1024);
	_debug.pushBack({ 0, 0 });	// Push two dummy entries to make search easier
	_debug.pushBack(AddrHeapPair{ (const roByte*)(-1), 0 });
#endif	// roMemoryProfilerDebug

	roAssert(!_profiler);
	_profiler = this;

	_tlsIndex = ::TlsAlloc();
	StackWalker::init();

	BsdSocket::initApplication();
	SockAddr addr(SockAddr::ipAny(), listeningPort);

	roStatus st;
	st = _listeningSocket.create(BsdSocket::TCP); if(!st) return st;
	st = _listeningSocket.bind(addr); if(!st) return st;
	st = _listeningSocket.listen(); if(!st) return st;
	st = _listeningSocket.setBlocking(false); if(!st) return st;

	{	// Show dialog
		WinDialogTemplate dialogTemplate("Memory profiler server", WS_CAPTION | DS_CENTER, 10, 10, 160, 50, NULL);
		String dialogStr;
		roVerify(strFormat(dialogStr, "Waiting for profiler connection on port {}", listeningPort));
		dialogTemplate.AddStatic(dialogStr.c_str(), WS_VISIBLE, 0, 2 + 3, 7, 200, 8, (WORD)-1);
		HWND hWnd = ::CreateDialogIndirect(::GetModuleHandle(NULL), dialogTemplate, NULL, (DLGPROC)dlgProc);
		::ShowWindow(hWnd, SW_SHOWDEFAULT);

		st = _acceptorSocket.create(BsdSocket::TCP); if(!st) return st;
		while(BsdSocket::inProgress(_listeningSocket.accept(_acceptorSocket))) {
			MSG msg = { 0 };
			if(::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				(void)TranslateMessage(&msg);
				(void)DispatchMessage(&msg);

				if(msg.message == WM_QUIT)
					return Status::user_abort;
			}
			TaskPool::sleep(10);
		}

		::DestroyWindow(hWnd);
	}

	{	// Send our process id to the client
		DWORD pid = GetCurrentProcessId();
		roSize size = sizeof(pid);
		_acceptorSocket.send(&pid, size);
	}

	st = _acceptorSocket.setBlocking(true);
	if(!st) return st;

	{	// Patching heap functions
		void* pHeapCreate, *pAlloc, *pReAlloc, *pFree, *pHeapDestroy;
		void* pNtAllocateVirtualMemory, *pNtFreeVirtualMemory;
		if(HMODULE h = GetModuleHandleA("ntdll.dll")) {
			pHeapCreate = GetProcAddress(h, "RtlCreateHeap");
			pAlloc = GetProcAddress(h, "RtlAllocateHeap");
			pReAlloc = GetProcAddress(h, "RtlReAllocateHeap");
			pFree = GetProcAddress(h, "RtlFreeHeap");	// RtlFreeStringRoutine and RtlpFreeHeap not exported
			pHeapDestroy = GetProcAddress(h, "RtlDestroyHeap");
			pNtAllocateVirtualMemory = GetProcAddress(h, "NtAllocateVirtualMemory");
			pNtFreeVirtualMemory = GetProcAddress(h, "NtFreeVirtualMemory");
		}
		else {
			pHeapCreate = &HeapCreate;
			pAlloc = &HeapAlloc;
			pReAlloc = &HeapReAlloc;
			pFree = &HeapFree;
			pHeapDestroy = &HeapDestroy;
			pNtAllocateVirtualMemory = NULL;
			pNtFreeVirtualMemory = NULL;
		}

		// Back up the original function and then do patching
		_orgHeapCreate = (MyHeapCreate)_functionPatcher.patch(pHeapCreate, &myHeapCreate);
		_orgHeapAlloc = (MyHeapAlloc)_functionPatcher.patch(pAlloc, &myHeapAlloc);
		_orgHeapFree = (MyHeapFree)_functionPatcher.patch(pFree, &myHeapFree);	// Must register Free before ReAlloc
		_orgHeapReAlloc = (MyHeapReAlloc)_functionPatcher.patch(pReAlloc, &myHeapReAlloc);
		_orgHeapDestroy = (MyHeapDestroy)_functionPatcher.patch(pHeapDestroy, &myHeapDestroy);
		_orgNtAllocateVirtualMemory = (MyNtAllocateVirtualMemory)_functionPatcher.patch(pNtAllocateVirtualMemory, &myNtAllocateVirtualMemory);
		_orgNtFreeVirtualMemory = (MyNtFreeVirtualMemory)_functionPatcher.patch(pNtFreeVirtualMemory, &myNtFreeVirtualMemory);
	}

	return Status::ok;
}

static bool _firstSeen(roUint64 hash)
{
	roAssert(_callstackHashMutex.isLocked());
	roUint64* h = roLowerBound(_callstackHash.begin(), _callstackHash.size(), hash);

	if(!h || (*h != hash)) {
		_callstackHash.insertSorted(hash);
		return true;
	}

	return false;
}

static void _send(TlsStruct* tls, roUint64 ticket, char cmd, void* memAddr, roSize memSize)
{
	ResetErrorCode scopedResetErrorCode;

	roUint64 hash = 0;

	// Right now we only need callstack for the allocation
	if(cmd == 'a' || cmd == 'V') {
		roUint64 count = tls->stackWalker.stackWalk(&tls->stackBuf[0], roCountof(tls->stackBuf), &hash);

		ScopeRecursiveLock lock(_callstackHashMutex);
		// The first time we save this particular callstack, send it to the client
		if(_firstSeen(hash)) {
			char c = 'h';

			Serializer s;
			s.setBuf(&tls->serializeBuf);

			s.io(c);
			s.ioVary(hash);
			s.ioVary(count);
			// Reverse the order so the root come first
			for(roSize i=0; i<count; ++i) {
				roUint64 addr = (roSize)tls->stackBuf[count - i - 1];
				s.ioVary(addr);
			}

			_regulator.send(tls);
			_regulator.flush();
		}
	}

	_ticketSystem.submit(tls, ticket, cmd, memAddr, memSize, hash);
}

// Slient out any events inside HeapCreate
HANDLE WINAPI myHeapCreate(ULONG flags, PVOID heapBase, SIZE_T reserveSize, SIZE_T commitSize, PVOID lock, PVOID parameters)
{
	TlsStruct* tls = _tlsStruct();

	if(!tls || tls->recurseCount > 0)
		return _orgHeapCreate(flags, heapBase, reserveSize, commitSize, lock, parameters);

	tls->recurseCount++;
	HANDLE ret = _orgHeapCreate(flags, heapBase, reserveSize, commitSize, lock, parameters);
	tls->recurseCount--;
	return ret;
}

LPVOID WINAPI myHeapAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();

	// HeapAlloc will invoke itself recursively, so we need a recursion counter
	if(!tls || tls->recurseCount > 0)
		return _orgHeapAlloc(hHeap, dwFlags, dwBytes);

	tls->recurseCount++;
	roUint64 ticket = TicketSystem::getTicket();
	void* ret = _orgHeapAlloc(hHeap, dwFlags, dwBytes);
	roAssert(::HeapSize(hHeap, dwFlags, ret) == dwBytes);

	_send(tls, ticket, 'a', ret, dwBytes);
	tls->recurseCount--;

	return ret;
}

LPVOID WINAPI myHeapReAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();
	if(!tls || tls->recurseCount > 0)
		return _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);

	tls->recurseCount++;
	roUint64 ticket1=0, ticket2=0;
	if(lpMem)	ticket1 = TicketSystem::getTicket();
	if(dwBytes)	ticket2 = TicketSystem::getTicket();
	roSize orgSize = ::HeapSize(hHeap, dwFlags, lpMem);	// NOTE: HeapSize doesn't calls SetLastError
	void* ret = _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
	roAssert(::HeapSize(hHeap, dwFlags, ret) == dwBytes);
	if(ret != lpMem || orgSize != dwBytes) {
		if(lpMem)	_send(tls, ticket1, 'f', lpMem, orgSize);
		if(dwBytes)	_send(tls, ticket2, 'a', ret, dwBytes);
		if(!lpMem)	_ticketSystem.voidTicket(tls, ticket1);
		if(!dwBytes)_ticketSystem.voidTicket(tls, ticket2);
	}
	else {
		_ticketSystem.voidTicket(tls, ticket1);
		_ticketSystem.voidTicket(tls, ticket2);
	}
	tls->recurseCount--;

	return ret;
}

LPVOID WINAPI myHeapFree(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem)
{
	TlsStruct* tls = _tlsStruct();
	if(!tls || tls->recurseCount > 0 || lpMem == NULL)
		return _orgHeapFree(hHeap, dwFlags, lpMem);

	tls->recurseCount++;
	roUint64 ticket = TicketSystem::getTicket();
	roSize orgSize = ::HeapSize(hHeap, dwFlags, lpMem);	// NOTE: HeapSize doesn't calls SetLastError
	void* ret = _orgHeapFree(hHeap, dwFlags, lpMem);
	_send(tls, ticket, 'f', lpMem, orgSize);
	tls->recurseCount--;

	return ret;
}

BOOL WINAPI myHeapDestroy(__in HANDLE hHeap)
{
	TlsStruct* tls = _tlsStruct();
	if(!tls || tls->recurseCount > 0)
		return _orgHeapDestroy(hHeap);

	{	ResetErrorCode scopedResetErrorCode;

		// Heap can be destory without first calling HeapFree
		if(::HeapLock(hHeap) == TRUE) {
			PROCESS_HEAP_ENTRY entry;
			entry.lpData = NULL;
			while (::HeapWalk(hHeap, &entry) != FALSE) {
				if(entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
					_send(tls, TicketSystem::getTicket(), 'f', entry.lpData, entry.cbData);
				}
			}
			::HeapUnlock(hHeap);
		}
	}

	tls->recurseCount++;
	BOOL ret = _orgHeapDestroy(hHeap);
	tls->recurseCount--;

	return ret;
}

NTSTATUS myNtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
{
	TlsStruct* tls = _tlsStruct();
	if(!tls || tls->recurseCount > 0 || !(AllocationType & MEM_COMMIT)) {
		NTSTATUS ret = _orgNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
		return ret;
	}

	tls->recurseCount++;
	roUint64 ticket = TicketSystem::getTicket();
	NTSTATUS ret = _orgNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);

	ResetErrorCode scopedResetErrorCode;
	if(ret == NO_ERROR) {
		if(*RegionSize)
			_send(tls, ticket, 'V', *BaseAddress, *RegionSize);
		else
			_ticketSystem.voidTicket(tls, ticket);
	}
	else
		_ticketSystem.voidTicket(tls, ticket);

	tls->recurseCount--;
	return ret;
}

NTSTATUS myNtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG Protect)
{
	TlsStruct* tls = _tlsStruct();
	if(!tls || tls->recurseCount > 0 || *BaseAddress == NULL) {
		NTSTATUS ret = _orgNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, Protect);
		return ret;
	}

	tls->recurseCount++;
	roUint64 ticket = TicketSystem::getTicket();
	NTSTATUS ret = _orgNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, Protect);

	ResetErrorCode scopedResetErrorCode;
	if(ret == NO_ERROR) {
		if(*RegionSize)
			_send(tls, ticket, 'v', *BaseAddress, *RegionSize);
		else
			_ticketSystem.voidTicket(tls, ticket);
	}
	else
		_ticketSystem.voidTicket(tls, ticket);

	tls->recurseCount--;
	return ret;
}

void MemoryProfiler::flush()
{
	if(_profiler)
		_regulator.flush();
}

void MemoryProfiler::shutdown()
{
	if(!_initCounter.tryShutdown())
		return;

	_regulator.flush();
	_functionPatcher.unpatchAll();

	roVerify(_acceptorSocket.shutDownReadWrite());
	_acceptorSocket.close();
	_listeningSocket.close();

	BsdSocket::closeApplication();

	_profiler = NULL;
}

}	// namespace ro
