#include "pch.h"
#include "roMemoryProfiler.h"
#include "roArray.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include <ImageHlp.h>

#pragma comment(lib, "imagehlp.lib")

namespace ro {

static MemoryProfiler* _profiler = NULL;

MemoryProfiler::MemoryProfiler()
{
	roAssert(!_profiler);
	_profiler = this;
}

MemoryProfiler::~MemoryProfiler()
{
	_profiler = NULL;
}

struct ScopeChangeProtection
{
	ScopeChangeProtection(void* original) {
		// Change rights on original function memory to execute/read/write.
		::VirtualQuery((void*)original, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
		::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);
	}

	~ScopeChangeProtection() {
		// Reset to original page protection.
		::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);
	}

	MEMORY_BASIC_INFORMATION mbi;
};

struct FunctionPatcher
{
	struct PathInfo
	{
		void* original;
		static const roSize MAX_PROLOGUE_CODE_SIZE = 64;
		unsigned char origByteCodes[MAX_PROLOGUE_CODE_SIZE];
		unsigned char jumpByteCodes[MAX_PROLOGUE_CODE_SIZE];
	};

	typedef roUint8* Address;
	#define IAX86_NEARJMP_OPCODE 0xe9
	#define JMP_CODE_SIZE 5
	#define MakeIAX86Offset(to,from) ((unsigned)((char*)(to)-(char*)(from)) - JMP_CODE_SIZE)

	void* patch(void* func, void* newFunc)
	{
		// The patchInfo was designed as static array, because we will change it's memory page protection
		roAssert(patchInfo.size() < patchInfo.capacity());
		if(patchInfo.size() >= patchInfo.capacity())
			return NULL;

		roAssert(func);
		if(!func) return NULL;
		MEMORY_BASIC_INFORMATION mbi;

		// Create a buffer of executable memory to store our jump table
		PathInfo& info = patchInfo.pushBack();
		info.original = func;

		Address uninfouedAddress = (Address)func + JMP_CODE_SIZE;
		Address infouedFromAddress = (Address)info.jumpByteCodes;
		memcpy(infouedFromAddress, func, JMP_CODE_SIZE);

		infouedFromAddress += JMP_CODE_SIZE;
		*(infouedFromAddress++) = IAX86_NEARJMP_OPCODE;
		*(roPtrInt*)infouedFromAddress = MakeIAX86Offset(uninfouedAddress, info.jumpByteCodes + JMP_CODE_SIZE);

		// Change rights on jump table to execute/read/write.
		::VirtualQuery(info.jumpByteCodes, &mbi, sizeof(mbi));
		::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);

		// Change rights on original function memory to execute/read/write.
		ScopeChangeProtection guard(func);

		// Save original code bytes for exit restoration
		memcpy(info.origByteCodes, func, JMP_CODE_SIZE);
		patchInfo.pushBack(info);

		// Write the jump instruction to jump to the replacement function
		Address patchloc = (Address)func;
		*patchloc++ = IAX86_NEARJMP_OPCODE;
		*(unsigned*)patchloc = MakeIAX86Offset(newFunc, func);

		return info.jumpByteCodes;
	}

	void unpatchAll()
	{
		for(roSize i=0; i < patchInfo.size(); ++i) {
			PathInfo& info = patchInfo[i];
			ScopeChangeProtection guard(info.original);
			memcpy(info.original, info.origByteCodes, JMP_CODE_SIZE);
		}
	}

	TinyArray<PathInfo, 8> patchInfo;
};

typedef LPVOID (WINAPI *MyHeapAlloc)(HANDLE, DWORD, SIZE_T);
typedef LPVOID (WINAPI *MyHeapReAlloc)(HANDLE, DWORD, LPVOID, SIZE_T);
typedef LPVOID (WINAPI *MyHeapFree)(HANDLE, DWORD, LPVOID);
typedef SIZE_T (WINAPI *MyHeapSize)(HANDLE, DWORD, LPCVOID);

LPVOID WINAPI myHeapAlloc(HANDLE, DWORD, SIZE_T);
LPVOID WINAPI myHeapReAlloc(HANDLE, DWORD, LPVOID, SIZE_T);
LPVOID WINAPI myHeapFree(HANDLE, DWORD, LPVOID);

FunctionPatcher _functionPatcher;

MyHeapAlloc		_orgHeapAlloc;
MyHeapReAlloc	_orgHeapReAlloc;
MyHeapFree		_orgHeapFree;
MyHeapSize		_orgHeapSize;

// I want to use __declspec(thread) but seems it cannot be accessed when a new thread is just created.
struct TlsStruct
{
	TlsStruct() : recurseCount(0) {}
	~TlsStruct() {}

	roSize recurseCount;
};	// TlsStruct

DWORD _tlsIndex = 0;
Mutex _tlsStructsMutex;
TinyArray<TlsStruct, 64> _tlsStructs;

TlsStruct* _tlsStruct()
{
	roAssert(_tlsIndex != 0);

	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));

	if(!tls) {
		ScopeLock lock(_tlsStructsMutex);

		// TODO: The current method didn't respect thread destruction
		roAssert(_tlsStructs.size() < _tlsStructs.capacity());
		if(_tlsStructs.size() >= _tlsStructs.capacity())
			return NULL;

		tls = &_tlsStructs.pushBack();
		::TlsSetValue(_tlsIndex, tls);
	}

	return tls;
}

Status MemoryProfiler::init(roUint16 listeningPort)
{
	_tlsIndex = ::TlsAlloc();

	SockAddr addr(SockAddr::ipAny(), listeningPort);

	if(_listeningSocket.create(BsdSocket::TCP) != 0) return Status::net_error;
	if(_listeningSocket.bind(addr) != 0) return Status::net_error;
	if(_listeningSocket.listen() != 0) return Status::net_error;
	if(_listeningSocket.setBlocking(false) != 0) return Status::net_error;

	if(_acceptorSocket.create(BsdSocket::TCP) != 0) return Status::net_error;
	while(BsdSocket::inProgress(_listeningSocket.accept(_acceptorSocket))) {
		TaskPool::sleep(10);
	}

	if(_acceptorSocket.setBlocking(true) != 0) return Status::net_error;

	{	// Patching heap functions
		void* pAlloc, *pReAlloc, *pFree;
		if(HMODULE h = GetModuleHandleA("ntdll.dll")) {
			pAlloc = GetProcAddress(h, "RtlAllocateHeap");
			pReAlloc = GetProcAddress(h, "RtlReAllocateHeap");
			pFree = GetProcAddress(h, "RtlFreeHeap");
			_orgHeapSize = (MyHeapSize)GetProcAddress(h, "RtlSizeHeap");
		}
		else {
			pAlloc = &HeapAlloc;
			pReAlloc = &HeapReAlloc;
			pFree = &HeapFree;
			_orgHeapSize = (MyHeapSize)&HeapSize;
		}

		// Back up the original function and then do patching
		_orgHeapAlloc = (MyHeapAlloc)_functionPatcher.patch(pAlloc, &myHeapAlloc);
		_orgHeapReAlloc = (MyHeapReAlloc)_functionPatcher.patch(pReAlloc, &myHeapReAlloc);
		_orgHeapFree = (MyHeapFree)_functionPatcher.patch(pFree, &myHeapFree);
	}

	return Status::ok;
}

void MemoryProfiler::tick()
{
	// Look for command from the client
}

struct StackTracer
{
	void init()
	{
	}

	static roUint8 stackWalk(void** address, roSize maxCount)
	{
		roSize reserveForNullTerminator = 1;
		USHORT frames = ::CaptureStackBackTrace(3, maxCount - reserveForNullTerminator, (void**)address, NULL);
		address[frames] = 0;
		return num_cast<roUint8>(frames);
	}

	static const roSize maxStackFrame = 64;
};

static void _send(TlsStruct* tls, char cmd, void* memAddr, roSize memSize)
{
	void* stack[64];
	roUint64 stack64[64];

	// Put our tls as a thread id, and make it looks like a call-stack entry
	stack[0] = tls;
	roUint8 count = StackTracer::stackWalk(&stack[1], roCountof(stack)-1) + 1;

	// Convert 32 bit address to 64 bits
	for(roSize i=0; i<=count; ++i)
		stack64[i] = (roUint64)stack[i];

	#pragma pack(1)
	struct Block { roUint8 cmd; roUint64 memAddr; roUint64 memSize; roUint16 stackSize; };
	Block block = { cmd, (roUint64)memAddr, memSize, num_cast<roUint16>(sizeof(roUint64) * count) };

	ScopeLock lock(_tlsStructsMutex);
	_profiler->_acceptorSocket.send(&block, sizeof(block));
	_profiler->_acceptorSocket.send(stack64, block.stackSize);
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
	_send(tls, 'a', ret, dwBytes);
//	TaskPool::sleep(10);
	tls->recurseCount--;

	return ret;
}

LPVOID WINAPI myHeapReAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();
	roAssert(tls);

	if(!tls || tls->recurseCount > 0)
		return _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);

	// NOTE: On VC 2005, orgHeapReAlloc() will not invoke HeapAlloc() and HeapFree(),
	// but it does on VC 2008
	tls->recurseCount++;
	roSize orgSize = _orgHeapSize(hHeap, dwFlags, lpMem);
	void* ret = _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
	_send(tls, 'f', lpMem, orgSize);
	_send(tls, 'a', ret, dwBytes);
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
	roSize orgSize = _orgHeapSize(hHeap, dwFlags, lpMem);
	void* ret = _orgHeapFree(hHeap, dwFlags, lpMem);
	_send(tls, 'f', lpMem, orgSize);
	tls->recurseCount--;

	return ret;
}

void MemoryProfiler::shutdown()
{
	_functionPatcher.unpatchAll();

	roVerify(_acceptorSocket.shutDownReadWrite() == 0);
	_acceptorSocket.close();
	_listeningSocket.close();
}

}	// namespace ro
