#include "pch.h"
#include "roMemoryProfiler.h"
#include "roSerializer.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roCpu.h"
#include <DbgHelp.h>
#include <Winsock2.h>
#include <Ntsecapi.h>

#include "roFuncPatcher.h"
#include "roStackWalker.h"

#pragma comment(lib, "DbgHelp.lib")

namespace ro {

static MemoryProfiler* _profiler = NULL;

FunctionPatcher _functionPatcher;

typedef LPVOID (WINAPI *MyHeapAlloc)(HANDLE, DWORD, SIZE_T);
typedef LPVOID (WINAPI *MyHeapReAlloc)(HANDLE, DWORD, LPVOID, SIZE_T);
typedef LPVOID (WINAPI *MyHeapFree)(HANDLE, DWORD, LPVOID);
typedef NTSTATUS (NTAPI *MyNtAlloc)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS (WINAPI *MyNtFree)(HANDLE, PVOID*, PSIZE_T, ULONG);

MyHeapAlloc		_orgHeapAlloc = NULL;
MyHeapReAlloc	_orgHeapReAlloc = NULL;
MyHeapFree		_orgHeapFree = NULL;
MyNtAlloc		_orgNtAlloc = NULL;
MyNtFree		_orgNtFree = NULL;

LPVOID WINAPI myHeapAlloc(HANDLE, DWORD, SIZE_T);
LPVOID WINAPI myHeapReAlloc(HANDLE, DWORD, LPVOID, SIZE_T);
LPVOID WINAPI myHeapFree(HANDLE, DWORD, LPVOID);
NTSTATUS NTAPI myNtAlloc(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
NTSTATUS WINAPI myNtFree(HANDLE, PVOID*, PSIZE_T, ULONG);

namespace {

// I want to use __declspec(thread) but seems it cannot be accessed when a new thread is just created.
struct TlsStruct
{
	TlsStruct() : recurseCount(0) {}
	~TlsStruct() {}

	roSize recurseCount;

	StackWalker stackWalker;

	static const roSize maxStackDepth = 512;
	void* stackBuf[maxStackDepth];
	TinyArray<roUint8, maxStackDepth * sizeof(void*)> serializeBuf;
};	// TlsStruct

}	// namespace

DWORD _tlsIndex = 0;
RecursiveMutex _mutex;
TinyArray<TlsStruct, 64> _tlsStructs;

TlsStruct* _tlsStruct()
{
	roAssert(_tlsIndex != 0);

	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));

	if(!tls) {
		ScopeRecursiveLock lock(_mutex);

		// TODO: The current method didn't respect thread destruction
		roAssert(_tlsStructs.size() < _tlsStructs.capacity());
		if(_tlsStructs.size() >= _tlsStructs.capacity())
			return NULL;

		if(!_tlsStructs.pushBack())
			return NULL;
		tls = &_tlsStructs.back();
		::TlsSetValue(_tlsIndex, tls);
	}

	return tls;
}

MemoryProfiler::MemoryProfiler()
{
}

MemoryProfiler::~MemoryProfiler()
{
	shutdown();
}

roUint64 _mainBeg = 0;
roUint64 _mainEnd = 0;
roUint64 _mainThreadId = 0;

Status MemoryProfiler::init(roUint16 listeningPort)
{
	roAssert(!_profiler);
	_profiler = this;

	_tlsIndex = ::TlsAlloc();
	StackWalker::init();

	SockAddr addr(SockAddr::ipAny(), listeningPort);

	if(_listeningSocket.create(BsdSocket::TCP) != 0) return Status::net_error;
	if(_listeningSocket.bind(addr) != 0) return Status::net_error;
	if(_listeningSocket.listen() != 0) return Status::net_error;
	if(_listeningSocket.setBlocking(false) != 0) return Status::net_error;

	if(_acceptorSocket.create(BsdSocket::TCP) != 0) return Status::net_error;
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

	{	// Send our process id to the client
		DWORD pid = GetCurrentProcessId();
		_profiler->_acceptorSocket.send(&pid, sizeof(pid));
	}

	if(_acceptorSocket.setBlocking(true) != 0) return Status::net_error;

	{	// Patching heap functions
		void* pAlloc, *pReAlloc, *pFree, *pNtAlloc, *pNtFree;
		if(HMODULE h = GetModuleHandleA("ntdll.dll")) {
			pAlloc = GetProcAddress(h, "RtlAllocateHeap");
			pReAlloc = GetProcAddress(h, "RtlReAllocateHeap");
			pFree = GetProcAddress(h, "RtlFreeHeap");
			pNtAlloc = GetProcAddress(h, "NtAllocateVirtualMemory");
			pNtFree = GetProcAddress(h, "NtFreeVirtualMemory");
		}
		else {
			pAlloc = &HeapAlloc;
			pReAlloc = &HeapReAlloc;
			pFree = &HeapFree;
			pNtAlloc = NULL;
			pNtFree = NULL;
		}

		// Back up the original function and then do patching
		_orgHeapAlloc = (MyHeapAlloc)_functionPatcher.patch(pAlloc, &myHeapAlloc);
		_orgHeapReAlloc = (MyHeapReAlloc)_functionPatcher.patch(pReAlloc, &myHeapReAlloc);
		_orgHeapFree = (MyHeapFree)_functionPatcher.patch(pFree, &myHeapFree);
//		_orgNtAlloc = (MyNtAlloc)_functionPatcher.patch(pNtAlloc, &myNtAlloc);
//		_orgNtFree = (MyNtFree)_functionPatcher.patch(pNtFree, &myNtFree);
	}

	return Status::ok;
}

void MemoryProfiler::tick()
{
	// Look for command from the client
	fd_set fdRead;
	FD_ZERO(&fdRead);
	#pragma warning(disable : 4389) // warning C4389: '==' : signed/unsigned mismatch
	FD_SET(_acceptorSocket.fd(), &fdRead);

	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));
	if(!tls)
		return;

	timeval tVal = { 0, 0 };
	while(select(1, &fdRead, NULL, NULL, &tVal) == 1) {
		roUint64 stackFrame;
		if(_acceptorSocket.receive(&stackFrame, sizeof(stackFrame)) == sizeof(stackFrame)) {
		}
	}
}

static void _send(TlsStruct* tls, char cmd, void* memAddr, roSize memSize)
{
	roSize count = 0;

	// Right now we only need callstack for the allocation
	if(cmd == 'a')
		count = tls->stackWalker.stackWalk(&tls->stackBuf[0], roCountof(tls->stackBuf));
	if(count > 2)
		count -= 2;

	Serializer s;
	tls->serializeBuf.clear();
	s.setBuf(&tls->serializeBuf);

	s.io(cmd);
	roUint64 memAddr64 = (roUint64)memAddr;
	s.ioVary(memAddr64);
	s.ioVary(memSize);

	if(cmd == 'a')
		s.ioVary(count);

	// Reverse the order so the root come first
	for(roSize i=0; i<count; ++i) {
		roSize addr = (roSize)tls->stackBuf[count - i];
		s.ioVary(addr);
	}

	ScopeRecursiveLock lock(_mutex);
	_profiler->_acceptorSocket.send(tls->serializeBuf.bytePtr(), tls->serializeBuf.sizeInByte());
}

LPVOID WINAPI myHeapAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	// HeapAlloc will invoke itself recursively, so we need a recursion counter
	if(!tls || tls->recurseCount > 0)
		return _orgHeapAlloc(hHeap, dwFlags, dwBytes);

	tls->recurseCount++;
	void* ret = _orgHeapAlloc(hHeap, dwFlags, dwBytes);
	roAssert(HeapSize(hHeap, dwFlags, ret) == dwBytes);
	_send(tls, 'a', ret, dwBytes);
	tls->recurseCount--;

	return ret;
}

LPVOID WINAPI myHeapReAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	if(!tls || tls->recurseCount > 0)
		return _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);

	tls->recurseCount++;
	roSize orgSize = HeapSize(hHeap, dwFlags, lpMem);
	void* ret = _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
	roAssert(HeapSize(hHeap, dwFlags, ret) == dwBytes);
	if(ret != lpMem || orgSize != dwBytes) {
		if(lpMem)	_send(tls, 'f', lpMem, orgSize);
		if(dwBytes)	_send(tls, 'a', ret, dwBytes);
	}
	tls->recurseCount--;

	return ret;
}

LPVOID WINAPI myHeapFree(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	if(!tls || tls->recurseCount > 0 || lpMem == NULL)
		return _orgHeapFree(hHeap, dwFlags, lpMem);

	tls->recurseCount++;
	roSize orgSize = HeapSize(hHeap, dwFlags, lpMem);
	void* ret = _orgHeapFree(hHeap, dwFlags, lpMem);
	_send(tls, 'f', lpMem, orgSize);
	tls->recurseCount--;

	return ret;
}

// Allocate on same address with same size is ok
// Allocate on same address with larger size will fail
NTSTATUS NTAPI myNtAlloc(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	if(!tls || tls->recurseCount > 0)
		return _orgNtAlloc(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);

	tls->recurseCount++;
	NTSTATUS ret = _orgNtAlloc(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
	if(0 == ret && BaseAddress) {
		if(AllocationType | MEM_COMMIT && *RegionSize > 0)
			_send(tls, 'a', *BaseAddress, *RegionSize);
	}
	tls->recurseCount--;

	return ret;
}

NTSTATUS WINAPI myNtFree(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	if(!tls || tls->recurseCount > 0)
		return _orgNtFree(ProcessHandle, BaseAddress, RegionSize, FreeType);

	tls->recurseCount++;
	NTSTATUS ret = _orgNtFree(ProcessHandle, BaseAddress, RegionSize, FreeType);
	if(0 == ret && BaseAddress) {
		if(FreeType | MEM_DECOMMIT)
			_send(tls, 'f', *BaseAddress, *RegionSize);
	}
	tls->recurseCount--;

	return ret;
}

void MemoryProfiler::shutdown()
{
	_functionPatcher.unpatchAll();

	roVerify(_acceptorSocket.shutDownReadWrite() == 0);
	_acceptorSocket.close();
	_listeningSocket.close();

	_profiler = NULL;
}

}	// namespace ro
