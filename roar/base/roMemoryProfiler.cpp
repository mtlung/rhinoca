#include "pch.h"
#include "roMemoryProfiler.h"
#include "roArray.h"
#include "roMutex.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

MemoryProfiler::MemoryProfiler()
{
}

MemoryProfiler::~MemoryProfiler()
{
}

struct ScopeChangeProtection
{
	ScopeChangeProtection(void* original) {
		// Change rights on orignal function memory to execute/read/write.
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
		// The patchInfo was designed as static array, because we will change it's memory page potection
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

		// Change rights on orignal function memory to execute/read/write.
		ScopeChangeProtection guard(func);

		// save original infoue code bytes for exit restoration
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

LPVOID WINAPI myHeapAlloc(HANDLE, DWORD, SIZE_T);
LPVOID WINAPI myHeapReAlloc(HANDLE, DWORD, LPVOID, SIZE_T);
LPVOID WINAPI myHeapFree(HANDLE, DWORD, LPVOID);

FunctionPatcher _functionPatcher;

MyHeapAlloc		_orgHeapAlloc;
MyHeapReAlloc	_orgHeapReAlloc;
MyHeapFree		_orgHeapFree;

// I want to use __declspec(thread) but seems it cannot be accessed when a new thread is just created.
struct TlsStruct
{
	TlsStruct() : recurseCount(0){}
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

		// TODO: The current method didn't repect thread destruction
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

	if(_acceptorSocket.create(BsdSocket::TCP) != 0) return Status::net_error;
//	if(_listeningSocket.accept(_acceptorSocket) != 0) return Status::net_error;

	{	// Patching heap functions
		void* pAlloc, *pReAlloc, *pFree;
		if(HMODULE h = GetModuleHandleA("ntdll.dll")) {
			pAlloc = GetProcAddress(h, "RtlAllocateHeap");
			pReAlloc = GetProcAddress(h, "RtlReAllocateHeap");
			pFree = GetProcAddress(h, "RtlFreeHeap");
		}
		else {
			pAlloc = &HeapAlloc;
			pReAlloc = &HeapReAlloc;
			pFree = &HeapFree;
		}

		// Back up the original function and then do patching
		_orgHeapAlloc = (MyHeapAlloc)_functionPatcher.patch(pAlloc, &myHeapAlloc);
		_orgHeapReAlloc = (MyHeapReAlloc)_functionPatcher.patch(pReAlloc, &myHeapReAlloc);
		_orgHeapFree = (MyHeapFree)_functionPatcher.patch(pFree, &myHeapFree);
	}

	return Status::ok;
}

__declspec(thread) int _recurseCount = 0;

LPVOID WINAPI myHeapAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();

	// HeapAlloc will invoke itself recursivly, so we need a recursion counter
	if(!tls || tls->recurseCount > 0)
		return _orgHeapAlloc(hHeap, dwFlags, dwBytes);

	tls->recurseCount++;
	void* p = _orgHeapAlloc(hHeap, dwFlags, dwBytes);
	tls->recurseCount--;

	return p;
}

LPVOID WINAPI myHeapReAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem, __in SIZE_T dwBytes)
{
	TlsStruct* tls = _tlsStruct();

	if(!tls || tls->recurseCount > 0 || lpMem == NULL)
		return _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);

	// NOTE: On VC 2005, orgHeapReAlloc() will not invoke HeapAlloc() and HeapFree(),
	// but it does on VC 2008
	tls->recurseCount++;
	void* p = _orgHeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
	tls->recurseCount--;

	return p;
}

LPVOID WINAPI myHeapFree(__in HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem)
{
	return _orgHeapFree(hHeap, dwFlags, lpMem);
}

void MemoryProfiler::shutdown()
{
	_functionPatcher.unpatchAll();

	roVerify(_acceptorSocket.shutDownReadWrite() == 0);
	_acceptorSocket.close();
	_listeningSocket.close();
}

void MemoryProfiler::tick()
{
}

}	// namespace ro
