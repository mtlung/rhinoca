#include "pch.h"
#include "roMemoryProfiler.h"
#include "roArray.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include <DbgHelp.h>
#include <Winsock2.h>
#include <Ntsecapi.h>

#pragma comment(lib, "DbgHelp.lib")

namespace ro {

namespace {

static MemoryProfiler* _profiler = NULL;

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
		if(!patchInfo.pushBack())
			return NULL;
		PathInfo& info = patchInfo.back();
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
typedef NTSTATUS(NTAPI *MyLdrLoadDll)(PWCHAR, ULONG, PUNICODE_STRING, PHANDLE);

FunctionPatcher _functionPatcher;

MyHeapAlloc		_orgHeapAlloc = NULL;
MyHeapReAlloc	_orgHeapReAlloc = NULL;
MyHeapFree		_orgHeapFree = NULL;
MyHeapSize		_orgHeapSize = NULL;
MyLdrLoadDll	_orgLdrLoadDll = NULL;

// I want to use __declspec(thread) but seems it cannot be accessed when a new thread is just created.
struct TlsStruct
{
	TlsStruct() : recurseCount(0) {}
	~TlsStruct() {}

	roSize recurseCount;
};	// TlsStruct

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

}	// namespace

LPVOID WINAPI myHeapAlloc(HANDLE, DWORD, SIZE_T);
LPVOID WINAPI myHeapReAlloc(HANDLE, DWORD, LPVOID, SIZE_T);
LPVOID WINAPI myHeapFree(HANDLE, DWORD, LPVOID);
NTSTATUS NTAPI myLdrLoadDll(PWCHAR, ULONG, PUNICODE_STRING, PHANDLE);

MemoryProfiler::MemoryProfiler()
{
}

MemoryProfiler::~MemoryProfiler()
{
	shutdown();
}

Status MemoryProfiler::init(roUint16 listeningPort)
{
	roAssert(!_profiler);
	_profiler = this;

	_tlsIndex = ::TlsAlloc();

	::SymInitialize(::GetCurrentProcess(), NULL, TRUE);

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

	if(_acceptorSocket.setBlocking(true) != 0) return Status::net_error;

	{	// Patching heap functions
		void* pAlloc, *pReAlloc, *pFree, *pLdrLoadDll;
		if(HMODULE h = GetModuleHandleA("ntdll.dll")) {
			pAlloc = GetProcAddress(h, "RtlAllocateHeap");
			pReAlloc = GetProcAddress(h, "RtlReAllocateHeap");
			pFree = GetProcAddress(h, "RtlFreeHeap");
			_orgHeapSize = (MyHeapSize)GetProcAddress(h, "RtlSizeHeap");
			pLdrLoadDll = GetProcAddress(h, "LdrLoadDll");
		}
		else {
			pAlloc = &HeapAlloc;
			pReAlloc = &HeapReAlloc;
			pFree = &HeapFree;
			_orgHeapSize = (MyHeapSize)&HeapSize;
			pLdrLoadDll = NULL;
		}

		// Back up the original function and then do patching
		_orgHeapAlloc = (MyHeapAlloc)_functionPatcher.patch(pAlloc, &myHeapAlloc);
		_orgHeapReAlloc = (MyHeapReAlloc)_functionPatcher.patch(pReAlloc, &myHeapReAlloc);
		_orgHeapFree = (MyHeapFree)_functionPatcher.patch(pFree, &myHeapFree);
		_orgLdrLoadDll = (MyLdrLoadDll)_functionPatcher.patch(pLdrLoadDll, &myLdrLoadDll);
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

	timeval tVal = { 0, 0 };
	while(select(1, &fdRead, NULL, NULL, &tVal) == 1) {
		roUint64 stackFrame;
		if(_acceptorSocket.receive(&stackFrame, sizeof(stackFrame)) == sizeof(stackFrame)) {
			char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
			PSYMBOL_INFO symbol = (PSYMBOL_INFO)buf;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			ScopeRecursiveLock lock(_mutex);
			roUint64 displacement = 0;
			if(!::SymFromAddr(::GetCurrentProcess(), stackFrame, &displacement, symbol)) {
				strcpy(symbol->Name, "no symbol");
				symbol->NameLen = num_cast<ULONG>(strlen(symbol->Name));
			}

			char cmd = 's';
			_acceptorSocket.send(&cmd, sizeof(cmd));
			_acceptorSocket.send(&stackFrame, sizeof(stackFrame));
			_acceptorSocket.send(&displacement, sizeof(displacement));
			roUint16 nameLen = num_cast<roUint16>(symbol->NameLen);
			_acceptorSocket.send(&nameLen, sizeof(nameLen));
			_acceptorSocket.send(&symbol->Name, nameLen);
		}
	}
}

struct StackTracer
{
	void init()
	{
	}

	static roUint8 stackWalk(void** address, roUint8 maxCount)
	{
		USHORT frames = ::CaptureStackBackTrace(5, maxCount, (void**)address, NULL);
		return num_cast<roUint8>(frames);
	}
};

static void _send(TlsStruct* tls, char cmd, void* memAddr, roSize memSize)
{
	void* stack[64];
	roUint64 stack64[64];

	roUint8 count = 0;

	// Right now we only need callstack for the allocation
	if(cmd == 'a')
		count = StackTracer::stackWalk(stack, roCountof(stack));
	if(count > 2)
		count -= 2;

	// Convert 32 bit address to 64 bits, and reverse the order so the root come first
	for(roSize i=0; i<count; ++i)
		stack64[i] = (roUint64)stack[count - i];

	#pragma pack(1)
	struct Block { roUint8 cmd; roUint64 memAddr; roUint64 memSize; roUint16 stackSize; };
	Block block = { cmd, (roUint64)memAddr, memSize, num_cast<roUint16>(sizeof(roUint64) * count) };

	ScopeRecursiveLock lock(_mutex);
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

NTSTATUS NTAPI myLdrLoadDll(PWCHAR PathToFile, ULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle)
{
	// Intercept any load module event and refresh the symbol table
	NTSTATUS ret = _orgLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
//	ScopeRecursiveLock lock(_mutex);
	::SymRefreshModuleList(::GetCurrentProcess());
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
