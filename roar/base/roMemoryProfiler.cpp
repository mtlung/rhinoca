#include "pch.h"
#include "roMemoryProfiler.h"
#include "roSerializer.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "roStopWatch.h"
#include "../platform/roCpu.h"
#include "../platform/roPlatformHeaders.h"

#include <DbgHelp.h>
#include <Winsock2.h>
#include <Ntsecapi.h>

#include "roFuncPatcher.h"
#include "roStackWalker.h"

#pragma comment(lib, "DbgHelp.lib")

namespace ro {

static DWORD			_tlsIndex = 0;
static RecursiveMutex	_mutex;
static FunctionPatcher	_functionPatcher;
static MemoryProfiler*	_profiler = NULL;

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

// To avoid the problem of calling malloc in the malloc callback,
// this Array class use VirtualAlloc as the memory source.
template<class T>
struct Array_VirtualAlloc : public IArray<T>
{
	Array_VirtualAlloc() { this->_data = NULL; this->_capacity = 0; }
	~Array_VirtualAlloc() { this->clear(); reserve(0, true); }
	override Status reserve(roSize newSize, bool force=false);
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
		::VirtualFree(_data, _capacity * sizeof(T), MEM_RELEASE);

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
			return flush(_profiler->_acceptorSocket);

		if(_stopWatch.getFloat() > 1)
			return flush(_profiler->_acceptorSocket);

		return true;
	}

	bool flush(BsdSocket& socket)
	{
		ScopeRecursiveLock lock(_mutex);

		roSize size = _buffer.sizeInByte();
		if(socket.send(_buffer.bytePtr(), size) != size)
			return false;

		_buffer.clear();
		_stopWatch.reset();
		return true;
	}

	StopWatch _stopWatch;
	Array_VirtualAlloc<char> _buffer;
	static const roSize _flushSize = 1024 * 512;
};

}	// namespace

static TinyArray<TlsStruct, 64> _tlsStructs;

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

static FlowRegulator _regulator;
static Array_VirtualAlloc<roUint64> _callstackHash;

static bool _firstSeen(roUint64 hash)
{
	ScopeRecursiveLock lock(_mutex);
	roUint64* h = roLowerBound(_callstackHash.begin(), _callstackHash.size(), hash);

	if(!h || (*h != hash)) {
		_callstackHash.insertSorted(hash);
		return true;
	}

	return false;
}

static void _send(TlsStruct* tls, char cmd, void* memAddr, roSize memSize)
{
	Serializer s;
	s.setBuf(&tls->serializeBuf);

	roUint64 hash = 0;

	// Right now we only need callstack for the allocation
	if(cmd == 'a') {
		roUint64 count = tls->stackWalker.stackWalk(&tls->stackBuf[0], roCountof(tls->stackBuf), &hash);
		if(count > 2)
			count -= 2;

		// The first time we save this particular callstack, send it to the client
		if(_firstSeen(hash)) {
			char c = 'h';
			s.setBuf(&tls->serializeBuf);

			s.io(c);
			s.ioVary(hash);
			s.ioVary(count);
			// Reverse the order so the root come first
			for(roSize i=0; i<count; ++i) {
				roUint64 addr = (roSize)tls->stackBuf[count - i];
				s.ioVary(addr);
			}

			_regulator.send(tls);
		}
	}

	s.setBuf(&tls->serializeBuf);
	s.io(cmd);
	roUint64 memAddr64 = (roUint64)memAddr;
	s.ioVary(memAddr64);
	s.ioVary(memSize);

	if(cmd == 'a')
		s.ioVary(hash);

	_regulator.send(tls);
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
