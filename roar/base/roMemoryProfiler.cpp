#include "pch.h"
#include "roMemoryProfiler.h"
#include "roArray.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roCpu.h"
#include "../platform/roPlatformHeaders.h"
#include <DbgHelp.h>
#include <Winsock2.h>
#include <Ntsecapi.h>

#if roCPU_LP32
#	include "hde/hde32.h"
#	include "hde/hde32.c"
#	define hde_disasm hde32_disasm
	typedef hde32s hdes;
#elif roCPU_LP64
#	include "hde/hde64.h"
#	include "hde/hde64.c"
#	define hde_disasm hde64_disasm
	typedef hde64s hdes;
#else
#	error
#endif

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
	struct PageAllocator
	{
		char* head;
		char* nextFree;
		roSize pageSize;
		roSize allocated;
	};

	struct PatchInfo
	{
		void* original;
		static const roSize MAX_PROLOGUE_CODE_SIZE = 64;
		TinyArray<unsigned char, 64> origByteCodes;
		void* jumpByteCodes;
		void* longJumpBuf;
	};

	void* allocatePage(void* nearAddr, roSize& size)
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		const roSize blockSize = sysInfo.dwAllocationGranularity;

		// Search for a page near by
		void* ret = NULL;
		char* addr = (char*)nearAddr;

		// Search towards higher memory address
		for(char* p = addr; !ret && p < addr + 0x30000000; p += blockSize)
			ret = ::VirtualAlloc(p, sysInfo.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		// Search towards lower memory address
		for(char* p = addr; !ret && p > addr - 0x30000000; p += blockSize)
			ret = ::VirtualAlloc(p, sysInfo.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

		if(ret)
			size = sysInfo.dwPageSize;

		return ret;
	}

	void* allocateExeBuffer(void* nearAddr, roSize size)
	{
		PageAllocator* alloc = NULL;
		char* addr = (char*)nearAddr;

		// Scan existing pages for available memory
		for(roSize i=0; i<allocatedPages.size(); ++i) {
			PageAllocator& a = allocatedPages[i];
			char* p = allocatedPages[i].nextFree;
			if(a.pageSize - a.allocated < size)
				continue;

			// nearAddr == NULL means no requirement on the address range
			if(!addr || (p > addr - 0x30000000 && p < addr + 0x30000000)) {
				alloc = &a;
				break;
			}
		}

		if(!alloc) {
			roSize pageSize = 0;
			void* page = allocatePage(nearAddr, pageSize);
			if(!page)
				return NULL;

			PageAllocator a = { (char*)page, (char*)page, pageSize, 0 };
			allocatedPages.pushBack(a);
			alloc = &allocatedPages.back();
		}

		roAssert(alloc->pageSize - alloc->allocated >= size);
		
		alloc->allocated += size;
		void* ret = alloc->nextFree;
		alloc->nextFree += size;

		return ret;
	}

	typedef roUint8* Address;

	// Reference: http://code.google.com/p/dropboxfilter/source/browse/Inject/mhook-lib/mhook.cpp?r=9abae8a80cf73d190cdc6aed8c46edbbb5b43078
	roSize emitJump(Address buf_, Address jumpDest)
	{
		const roSize relativeJmpCodeSize = 5;	// 0xE9 (1 byte) + jump address (4 bytes)
		Address buf = buf_;
		Address jumpSrc = buf + relativeJmpCodeSize;
		roSize absDiff = jumpSrc > jumpDest ? jumpSrc - jumpDest : jumpDest - jumpSrc;

		// With a small jump distance we can use the relative jump
		if(absDiff <= 0x7FFF0000) {
			buf[0] = 0xE9;
			buf++;

			*(roInt32*)buf = roInt32(jumpDest - jumpSrc);
			buf += sizeof(roInt32);
		}
		// Otherwise jump to absolute address
		else {
			buf[0] = 0xFF;
			buf[1] = 0x25;
			buf += 2;

			*(roInt32*)buf = 0;
			buf += sizeof(roInt32);

			*(void**)(buf) = (void*)jumpDest;
			buf += sizeof(void*);
		}

		return buf - buf_;
	}

	// We want to intercept the origanl function with our target function,
	// and allow target function to call the original function.
	// ------------                   --------
	// |   Orig   |                   |target|
	// |   ...    |	                  | ...  |
	//
	// It is done by patching the original funtion with jmp instruction like follow:
	//
	// ------------     ----------    --------
	// |short jmp |---->|long jmp|--->|target|
	// |   ...    |<-|  ----------    | ...  |    ----------
	// |          |  |                | call |--->|  Orig  |
	//               |                         |--|long jmp|
	//               |-------------------------|  ----------
	//
	// We need to use a short jmp (+-2G address) at the patch site because
	// otherwise we need a 14 bytes instruction (on x64), while creates
	// problem for example the whole original function is less than 14 bytes.
	// How do we make sure a short jmp can be used? Thanks to VirtualAlloc().
	void* patch(void* func, void* newFunc)
	{
		// The patchInfo was designed as static array, because we will change it's memory page protection
		roAssert(patchInfo.size() < patchInfo.capacity());
		if(patchInfo.size() >= patchInfo.capacity())
			return NULL;

		roAssert(func);
		if(!func) return NULL;

		// Create a buffer of executable memory to store our jump table
		if(!patchInfo.pushBack())
			return NULL;
		PatchInfo& info = patchInfo.back();

		// Get the instruction boundary of the original function and copy that header to origByteCodes
		const roSize relativeJmpCodeSize = 5;
		{
			hdes hs;
			Address f = (Address)func;
			while((f - (Address)func) < relativeJmpCodeSize) {
				// TODO: Should abort the patching if any branch instruction encountered
				f += hde_disasm(f, &hs);
			}

			info.original = func;
			info.origByteCodes.pushBack((Address)func, f - (Address)func);
		}

		// Patch header of the original function
		{
			// Change rights on original function memory to execute/read/write.
			ScopeChangeProtection guard(func);

			// Allocate buffer for the jump buffer
			info.longJumpBuf = allocateExeBuffer(func, 16);

			// Write the jump instruction to jump from func to longJumpBuf
			emitJump((Address)func, (Address)info.longJumpBuf);

			// Write the jump instruction to jump from longJumpBuf to newFunc
			emitJump((Address)info.longJumpBuf, (Address)newFunc);
		}

		// Copy the header of the original function to our stub function
		{
			roSize headerSize = info.origByteCodes.size();

			info.jumpByteCodes = allocateExeBuffer(NULL, 16);
			Address infouedFromAddress = (Address)info.jumpByteCodes;
			memcpy(infouedFromAddress, info.origByteCodes.bytePtr(), headerSize);
			infouedFromAddress += headerSize;

			Address uninfouedAddress = (Address)func + headerSize;
			emitJump(infouedFromAddress, uninfouedAddress);
		}

		return info.jumpByteCodes;
	}

	void unpatchAll()
	{
		for(roSize i=0; i < patchInfo.size(); ++i) {
			PatchInfo& info = patchInfo[i];
			ScopeChangeProtection guard(info.original);
			memcpy(info.original, info.origByteCodes.bytePtr(), info.origByteCodes.size());
		}
		patchInfo.clear();

		for(roSize i=0; i<allocatedPages.size(); ++i) {
			roVerify(SUCCEEDED(VirtualFree(allocatedPages[i].head, 0, MEM_RELEASE)));
		}
		allocatedPages.clear();
	}

	TinyArray<PatchInfo, 8> patchInfo;
	TinyArray<PageAllocator, 8> allocatedPages;
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


void MemoryProfiler::tick()
{
	// Look for command from the client
	fd_set fdRead;
	FD_ZERO(&fdRead);
	#pragma warning(disable : 4389) // warning C4389: '==' : signed/unsigned mismatch
	FD_SET(_acceptorSocket.fd(), &fdRead);

	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));
	tls->recurseCount++;	// Exclude the memory overhead cause by the debug symbols

	static bool _symInited = false;
	HANDLE _process = ::GetCurrentProcess();

	timeval tVal = { 0, 0 };
	while(select(1, &fdRead, NULL, NULL, &tVal) == 1) {
		roUint64 stackFrame;
		if(_acceptorSocket.receive(&stackFrame, sizeof(stackFrame)) == sizeof(stackFrame)) {
			if(!_symInited) {
				::SymInitialize(_process, NULL, TRUE);
				_symInited = true;
			}

			char buf[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR)];
			PIMAGEHLP_SYMBOL64 symbol = (PIMAGEHLP_SYMBOL64)buf;
			symbol->SizeOfStruct = sizeof(buf);
			symbol->MaxNameLength = MAX_SYM_NAME;

			ScopeRecursiveLock lock(_mutex);
			roUint64 displacement = 0;
			if(!::SymGetSymFromAddr64(_process, DWORD64(stackFrame), &displacement, symbol)) {
				strcpy(symbol->Name, "no symbol");
			}

			char cmd = 's';
			_acceptorSocket.send(&cmd, sizeof(cmd));
			_acceptorSocket.send(&stackFrame, sizeof(stackFrame));
			_acceptorSocket.send(&displacement, sizeof(displacement));
			roUint16 nameLen = num_cast<roUint16>(strlen(symbol->Name));
			_acceptorSocket.send(&nameLen, sizeof(nameLen));
			_acceptorSocket.send(&symbol->Name, nameLen);
		}
	}

	if(_symInited) {
		::SymCleanup(_process);
		_symInited = false;
	}

	tls->recurseCount--;
}

struct StackTracer
{
	void init()
	{
	}

	static roUint8 stackWalk(void** address, roUint8 maxCount)
	{
		USHORT frames = ::CaptureStackBackTrace(2, maxCount, (void**)address, NULL);
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
	roSize orgSize = HeapSize(hHeap, dwFlags, lpMem);
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
	roSize orgSize = HeapSize(hHeap, dwFlags, lpMem);
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

	_profiler = NULL;
}

}	// namespace ro
