#ifndef __roStackWalker_h__
#define __roStackWalker_h__

#include "../platform/roCompiler.h"
#include "../platform/roPlatformHeaders.h"

//////////////////////////////////////////////////////////////////////////

#if defined(_WIN64)

// Declarations for RtlVirtualUnwind() and RtlLookupFunctionEntry()

#ifndef UNWIND_HISTORY_TABLE_SIZE

#define OLD_WINDOWS_SDK_VERSION

typedef struct _KNONVOLATILE_CONTEXT_POINTERS {
	union {
		PM128A FloatingContext[16];
		struct {
			PM128A Xmm0;	PM128A Xmm1;
			PM128A Xmm2;	PM128A Xmm3;
			PM128A Xmm4;	PM128A Xmm5;
			PM128A Xmm6;	PM128A Xmm7;
			PM128A Xmm8;	PM128A Xmm9;
			PM128A Xmm10;	PM128A Xmm11;
			PM128A Xmm12;	PM128A Xmm13;
			PM128A Xmm14;	PM128A Xmm15;
		};
	};

	union {
		PULONG64 IntegerContext[16];
		struct {
			PULONG64 Rax;	PULONG64 Rcx;
			PULONG64 Rdx;	PULONG64 Rbx;
			PULONG64 Rsp;	PULONG64 Rbp;
			PULONG64 Rsi;	PULONG64 Rdi;
			PULONG64 R8;	PULONG64 R9;
			PULONG64 R10;	PULONG64 R11;
			PULONG64 R12;	PULONG64 R13;
			PULONG64 R14;	PULONG64 R15;
		};
	};
} KNONVOLATILE_CONTEXT_POINTERS, *PKNONVOLATILE_CONTEXT_POINTERS;

typedef EXCEPTION_DISPOSITION (*PEXCEPTION_ROUTINE) (
	IN struct _EXCEPTION_RECORD *ExceptionRecord,
	IN PVOID establisherFrame,
	IN OUT struct _CONTEXT *ContextRecord,
	IN OUT PVOID DispatcherContext
	);

#define UNWIND_HISTORY_TABLE_SIZE 12

typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
	ULONG64 imageBase;
	PRUNTIME_FUNCTION FunctionEntry;
} UNWIND_HISTORY_TABLE_ENTRY, *PUNWIND_HISTORY_TABLE_ENTRY;

#define UNWIND_HISTORY_TABLE_NONE 0
#define UNWIND_HISTORY_TABLE_GLOBAL 1
#define UNWIND_HISTORY_TABLE_LOCAL 2

typedef struct _UNWIND_HISTORY_TABLE {
	ULONG Count;
	UCHAR Search;
	ULONG64 LowAddress;
	ULONG64 HighAddress;
	UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;

#endif	// UNWIND_HISTORY_TABLE_SIZE

typedef PEXCEPTION_ROUTINE (WINAPI *RtlVirtualUnwindFunc)(
	ULONG, ULONG64, ULONG64, PRUNTIME_FUNCTION, PCONTEXT,
	PVOID*, PULONG64, PKNONVOLATILE_CONTEXT_POINTERS
);
static RtlVirtualUnwindFunc _RtlVirtualUnwind = NULL;

typedef PRUNTIME_FUNCTION (WINAPI *RtlLookupFunctionEntryFunc)(
	ULONG64, PULONG64, PUNWIND_HISTORY_TABLE HistoryTable
);
static RtlLookupFunctionEntryFunc _RtlLookupFunctionEntry = NULL;

#endif

//////////////////////////////////////////////////////////////////////////

namespace ro {

struct StackWalker
{
	static void init();

	roSize stackWalk	(void** address, roSize maxCount, roUint64* outHash);
	roSize stackWalk_v0	(void** address, roSize maxCount, roUint64* outHash);
	roSize stackWalk_v1	(void** address, roSize maxCount, roUint64* outHash);
	roSize stackWalk_v2	(void** address, roSize maxCount, roUint64* outHash);
	roSize stackWalk_v3	(void** address, roSize maxCount, roUint64* outHash);

	static const roSize maxStackFrame = 64;
};	// StackWalker

}	// namespace ro

#endif	// __roStackWalker_h__
