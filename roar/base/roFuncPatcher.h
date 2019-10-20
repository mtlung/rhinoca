#ifndef __roFuncPatcher_h__
#define __roFuncPatcher_h__

#include "roArray.h"

typedef size_t roSize;
typedef __int32 roInt32;
typedef unsigned __int8 roUint8;
typedef unsigned __int16 roUint16;
typedef unsigned __int32 roUint32;
typedef unsigned __int64 roUint64;

#if defined(_WIN64)
#	include "hde/hde64.h"
#	include "hde/hde64.c"
#	define hde_disasm hde64_disasm
typedef hde64s hdes;
#elif defined(_WIN32)
#	include "hde/hde32.h"
#	include "hde/hde32.c"
#	define hde_disasm hde32_disasm
typedef hde32s hdes;
#else
#	error
#endif

namespace ro {

struct FunctionPatcher
{
	void* patch(void* func, void* newFunc);
	void unpatchAll();

// Private
	typedef roUint8* Address;

	void* _allocatePage(void* nearAddr, roSize& size);
	void* _allocateExeBuffer(void* nearAddr, roSize size);
	roSize _emitJump(Address buf_, Address jumpDest);

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
		Array<unsigned char> origByteCodes;
		void* jumpByteCodes;
		void* longJumpBuf;
	};

	struct ScopeChangeProtection
	{
		ScopeChangeProtection(void* original) {
			// Change rights on original function memory to execute/read/write.
			// NOTE: On x64 build we need 16 byte member alignment for MEMORY_BASIC_INFORMATION
			::VirtualQuery((void*)original, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
			::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);
		}

		~ScopeChangeProtection() {
			// Reset to original page protection.
			::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);
		}

		MEMORY_BASIC_INFORMATION mbi;
	};

	TinyArray<PatchInfo, 16> _patchInfo;
	TinyArray<PageAllocator, 16> _allocatedPages;
};

void* FunctionPatcher::_allocatePage(void* nearAddr, roSize& size)
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

void* FunctionPatcher::_allocateExeBuffer(void* nearAddr, roSize size)
{
	PageAllocator* alloc = NULL;
	char* addr = (char*)nearAddr;

	// Scan existing pages for available memory
	for(PageAllocator& a : _allocatedPages) {
		char* p = a.nextFree;
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
		void* page = _allocatePage(nearAddr, pageSize);
		if(!page)
			return NULL;

		PageAllocator a = { (char*)page, (char*)page, pageSize, 0 };
		_allocatedPages.pushBack(a);
		alloc = &_allocatedPages.back();
	}

	alloc->allocated += size;
	void* ret = alloc->nextFree;
	alloc->nextFree += size;

	return ret;
}

// Reference: http://code.google.com/p/dropboxfilter/source/browse/Inject/mhook-lib/mhook.cpp?r=9abae8a80cf73d190cdc6aed8c46edbbb5b43078
roSize FunctionPatcher::_emitJump(Address buf_, Address jumpDest)
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

// We want to intercept the original function with our target function,
// and allow target function to call the original function.
// ------------                   --------
// |   Orig   |                   |target|
// |   ...    |	                  | ...  |
//
// It is done by patching the original function with jmp instruction like follow:
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
void* FunctionPatcher::patch(void* func, void* newFunc)
{
	roAssert(func);
	if(!func) return NULL;

	// Create a buffer of executable memory to store our jump table
	_patchInfo.pushBack(PatchInfo());
	PatchInfo& info = _patchInfo.back();

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
		info.origByteCodes.assign((Address)func, (Address)f);
	}

	// Patch header of the original function
	{
		// Change rights on original function memory to execute/read/write.
		ScopeChangeProtection guard(func);

		// Allocate buffer for the jump buffer
		info.longJumpBuf = _allocateExeBuffer(func, 16);

		// Write the jump instruction to jump from func to longJumpBuf
		_emitJump((Address)func, (Address)info.longJumpBuf);

		// Write the jump instruction to jump from longJumpBuf to newFunc
		_emitJump((Address)info.longJumpBuf, (Address)newFunc);
	}

	// Copy the header of the original function to our stub function
	{
		roSize headerSize = info.origByteCodes.size();

		info.jumpByteCodes = _allocateExeBuffer(NULL, 32);	// In most cases 16 bytes is enough but some need 32
		Address infouedFromAddress = (Address)info.jumpByteCodes;
		memcpy(infouedFromAddress, &info.origByteCodes.front(), headerSize);
		infouedFromAddress += headerSize;

		Address uninfouedAddress = (Address)func + headerSize;
		_emitJump(infouedFromAddress, uninfouedAddress);
	}

	return info.jumpByteCodes;
}

void FunctionPatcher::unpatchAll()
{
	for(PatchInfo& info : _patchInfo) {
		ScopeChangeProtection guard(info.original);
		memcpy(info.original, &info.origByteCodes.front(), info.origByteCodes.size());
	}
	_patchInfo.clear();

	for(PageAllocator& i : _allocatedPages) {
		VirtualFree(i.head, 0, MEM_RELEASE);
	}
	_allocatedPages.clear();
}

}	// namespace ro

#endif	// __roFuncPatcher_h__
