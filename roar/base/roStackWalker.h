#ifndef __roStackWalker_h__
#define __roStackWalker_h__

#include <WinNT.h>

typedef size_t roSize;
typedef __int32 roInt32;
typedef unsigned __int8 roUint8;
typedef unsigned __int16 roUint16;
typedef unsigned __int32 roUint32;
typedef unsigned __int64 roUint64;

//////////////////////////////////////////////////////////////////////////

typedef USHORT (WINAPI *CaptureStackBackTraceFunc)(
	__in ULONG framesToSkip,
	__in ULONG framesToCapture,
	__out_ecount(framesToCapture) PVOID* backTrace,
	__out_opt PULONG backTraceHash
);
static CaptureStackBackTraceFunc _captureStackBackTrace = NULL;

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

	roSize stackWalk	(void** address, roSize maxCount);
	roSize stackWalk_v1	(void** address, roSize maxCount);
	roSize stackWalk_v2	(void** address, roSize maxCount);
	roSize stackWalk_v3	(void** address, roSize maxCount);

	static const roSize maxStackFrame = 64;
};

void StackWalker::init()
{
	if(!_captureStackBackTrace)
		_captureStackBackTrace = (CaptureStackBackTraceFunc)(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "RtlCaptureStackBackTrace"));

#if defined(_WIN64)
	if(!_RtlVirtualUnwind)
		_RtlVirtualUnwind = (RtlVirtualUnwindFunc)(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlVirtualUnwind"));

	if(!_RtlLookupFunctionEntry)
		_RtlLookupFunctionEntry = (RtlLookupFunctionEntryFunc)(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlLookupFunctionEntry"));
#endif
}

roSize StackWalker::stackWalk(void** address, roSize maxCount)
{
	return stackWalk_v1(address, maxCount);
}

roSize StackWalker::stackWalk_v1(void** address, roSize maxCount)
{
	// NOTE: We may need other accurate stack trace method, especially
	// when "Omit Frame Pointer Optimization" has been turn on.
	// http://www.nynaeve.net/?p=97
	// http://www.yosefk.com/blog/getting-the-call-stack-without-a-frame-pointer.html
	return _captureStackBackTrace(2, (DWORD)maxCount, address, NULL);
}

roSize StackWalker::stackWalk_v2(void** address, roSize maxCount)
{
	CONTEXT ctx;
	::RtlCaptureContext(&ctx);

	roSize depth = 0;
	STACKFRAME64 sf;
	BOOL success = TRUE;
	DWORD machineType = IMAGE_FILE_MACHINE_I386;

	HANDLE hProcess = ::GetCurrentProcess();
	HANDLE hThread = ::GetCurrentThread();
	__try
	{
		// Initialize the STACKFRAME structure.
		memset(&sf, 0, sizeof(sf));
		sf.AddrPC.Mode		= AddrModeFlat;
		sf.AddrStack.Mode	= AddrModeFlat;
		sf.AddrFrame.Mode	= AddrModeFlat;
#ifdef _WIN64
		sf.AddrPC.Offset	= ctx.Rip;
		sf.AddrStack.Offset	= ctx.Rsp;
		sf.AddrFrame.Offset	= ctx.Rbp;
		machineType			= IMAGE_FILE_MACHINE_AMD64;
#else
		sf.AddrPC.Offset	= ctx.Eip;
		sf.AddrStack.Offset	= ctx.Esp;
		sf.AddrFrame.Offset	= ctx.Ebp;
#endif

		// Walk the stack one frame at a time.
		while(success && (depth < maxCount))
		{
			success = ::StackWalk64(machineType, 
				hProcess, hThread, 
				&sf,
				&ctx,
				NULL,
				&::SymFunctionTableAccess64, &::SymGetModuleBase64,
				NULL
			);

			address[depth++] = (void*)sf.AddrPC.Offset;

			if(!success)
				break;

			// Stop if the frame pointer or address is NULL.
			if(sf.AddrFrame.Offset == 0 || sf.AddrPC.Offset == 0)
				break;
		}
	} 
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		// We need to catch any exceptions within this function so they don't get sent to 
		// the engine's error handler, hence causing an infinite loop.
		return EXCEPTION_EXECUTE_HANDLER;
	}

	if(depth < maxCount)
		address[depth] = NULL;

	return depth;
}
/*
roSize StackWalker::stackWalk_v2(void** address, roSize maxCount)
{
	CONTEXT Context;
	RtlCaptureContext( &Context );

	STACKFRAME64		StackFrame64;
	HANDLE				ProcessHandle;
	HANDLE				ThreadHandle;
	unsigned long		LastError;
	BOOL				bStackWalkSucceeded	= TRUE;
	DWORD				CurrentDepth		= 0;
	DWORD				MachineType			= IMAGE_FILE_MACHINE_I386;
	CONTEXT				ContextCopy = Context;

	__try
	{
		// Get context, process and thread information.
		ProcessHandle	= ::GetCurrentProcess();
		ThreadHandle	= ::GetCurrentThread();

		// Zero out stack frame.
		memset( &StackFrame64, 0, sizeof(StackFrame64) );

		// Initialize the STACKFRAME structure.
		StackFrame64.AddrPC.Mode         = AddrModeFlat;
		StackFrame64.AddrStack.Mode      = AddrModeFlat;
		StackFrame64.AddrFrame.Mode      = AddrModeFlat;
#ifdef _WIN64
		StackFrame64.AddrPC.Offset       = Context.Rip;
		StackFrame64.AddrStack.Offset    = Context.Rsp;
		StackFrame64.AddrFrame.Offset    = Context.Rbp;
		MachineType                      = IMAGE_FILE_MACHINE_AMD64;
#else
		StackFrame64.AddrPC.Offset       = Context.Eip;
		StackFrame64.AddrStack.Offset    = Context.Esp;
		StackFrame64.AddrFrame.Offset    = Context.Ebp;
#endif

		// Walk the stack one frame at a time.
		while( bStackWalkSucceeded && (CurrentDepth < maxCount) )
		{
			bStackWalkSucceeded = StackWalk64(MachineType, 
				ProcessHandle, 
				ThreadHandle, 
				&StackFrame64,
				&ContextCopy,
				NULL,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				NULL
				);

			address[CurrentDepth++] = (void*)StackFrame64.AddrPC.Offset;

			if( !bStackWalkSucceeded  )
			{
				// StackWalk failed! give up.
				LastError = GetLastError( );
				break;
			}

			// Stop if the frame pointer or address is NULL.
			if( StackFrame64.AddrFrame.Offset == 0 || StackFrame64.AddrPC.Offset == 0 )
			{
				break;
			}
		}
	} 
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		// We need to catch any exceptions within this function so they don't get sent to 
		// the engine's error handler, hence causing an infinite loop.
		return EXCEPTION_EXECUTE_HANDLER;
	} 

	// NULL out remaining entries.
	roUint8 ret = (roUint8)CurrentDepth;
	for ( ; CurrentDepth<maxCount; CurrentDepth++ )
	{
		address[CurrentDepth] = NULL;
	}

	return ret;
}*/

#if defined(_WIN64)

roSize StackWalker::stackWalk_v3(void** address, roSize maxCount)
{
	// Adopted from:
	// http://www.nynaeve.net/Code/StackWalk64.cpp
	// http://mockitnow.googlecode.com/svn-history/r13/trunk/Library/Win64/StackWalker.cpp
	CONTEXT ctx;
	::RtlCaptureContext(&ctx);

	ULONG64 imageBase;
	PVOID handlerData;
	ULONG64 establisherFrame;
	PRUNTIME_FUNCTION runtimeFunction;
	KNONVOLATILE_CONTEXT_POINTERS nvContext;

	int frame = 0;
	while(true)
	{
		// Try to look up unwind metadata for the current function.
		runtimeFunction = _RtlLookupFunctionEntry(ctx.Rip, &imageBase, NULL);

		if(!runtimeFunction)
		{
			//If we don't have a RUNTIME_FUNCTION, then we've encountered
			//a leaf function.  Adjust the stack appropriately.
			ctx.Rip = (ULONG64)(*(PULONG64)ctx.Rsp);
			ctx.Rsp += 8;
		}
		else
		{
			// Otherwise, call upon RtlVirtualUnwind to execute the unwind for us.
			RtlZeroMemory(&nvContext, sizeof(KNONVOLATILE_CONTEXT_POINTERS));
			_RtlVirtualUnwind(0, imageBase, ctx.Rip, runtimeFunction, &ctx, &handlerData,
				&establisherFrame, &nvContext);
		}

		//If we reach an RIP of zero, this means that we've walked off the end
		//of the call stack and are done.
		if(!ctx.Rip)
			break;

		if(runtimeFunction)
			address[frame++] = (void*)(imageBase + runtimeFunction->BeginAddress);
		else
			address[frame++] = (void*)ctx.Rip;
	}

	return frame;
}

#endif

}	// namespace ro

#endif	// __roStackWalker_h__
