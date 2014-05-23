#include "pch.h"
#include "roStackWalker.h"
#include "roUtility.h"
#include <DbgHelp.h>

typedef USHORT (WINAPI *CaptureStackBackTraceFunc)(
	__in ULONG framesToSkip,
	__in ULONG framesToCapture,
	__out_ecount(framesToCapture) PVOID* backTrace,
	__out_opt PULONG backTraceHash
);
static CaptureStackBackTraceFunc _captureStackBackTrace = NULL;

namespace ro {

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

roSize StackWalker::stackWalk(void** address, roSize maxCount, roUint64* outHash)
{
	return stackWalk_v1(address, maxCount, outHash);
}

roSize StackWalker::stackWalk_v0(void** address, roSize maxCount, roUint64* outHash)
{
	roSize count = 0;

#if roCPU_x86 && roCOMPILER_VC
	void* myEbp = 0;
	__asm { mov myEbp, ebp }

	for(; myEbp; ++count)
	{
		void** frame = (void**)myEbp;
		void* nextFrame = frame[0];
		void* funcAddress = frame[1];
		address[count] = frame;
		myEbp = (void**)nextFrame;
	}
#endif

	return count;
}

roSize StackWalker::stackWalk_v1(void** address, roSize maxCount, roUint64* outHash)
{
	roZeroMemory(address, maxCount * sizeof(address));
	// NOTE: We may need other accurate stack trace method, especially
	// when "Omit Frame Pointer Optimization" has been turn on.
	// http://www.nynaeve.net/?p=97
	// http://www.yosefk.com/blog/getting-the-call-stack-without-a-frame-pointer.html
	ULONG hash;
	roSize ret = _captureStackBackTrace(2, (DWORD)maxCount, address, &hash);
	if(outHash)
		*outHash = hash;
	return ret;
}

roSize StackWalker::stackWalk_v2(void** address, roSize maxCount, roUint64* outHash)
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
roSize StackWalker::stackWalk_v2(void** address, roSize maxCount, roUint64* outHash)
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

roSize StackWalker::stackWalk_v3(void** address, roSize maxCount, roUint64* outHash)
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
			roMemZeroStruct(nvContext);
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
