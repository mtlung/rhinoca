#include "pch.h"
#include <assert.h>
#include "roPlatformHeaders.h"

void _roAssert(const wchar_t* expression, const wchar_t* file, unsigned line)
{
#if roDEBUG
	_wassert(expression, file, line);
#endif
}

void roDebugBreak(bool doBreak)
{
	static bool canChangeMeInDebugger = true;
	if(doBreak && canChangeMeInDebugger && IsDebuggerPresent())
		__debugbreak();
}

#define IS_ALIGNED(val, align) (((val) & ((align) - 1)) == 0)

bool _roSetDataBreakpoint(unsigned index, void* address, unsigned size)
{
	// DR7 bits are as follows:
	//
	//   31              23              15              7             0
	//  +---+---+---+---+---+---+---+---+---+-+-----+-+-+-+-+-+-+-+-+-+-+
	//  |LEN|R/W|LEN|R/W|LEN|R/W|LEN|R/W|   | |     |G|L|G|L|G|L|G|L|G|L|
	//  |   |   |   |   |   |   |   |   |0 0|0|0 0 0| | | | | | | | | | | DR7
	//  | 3 | 3 | 2 | 2 | 1 | 1 | 0 | 0 |   | |     |E|E|3|3|2|2|1|1|0|0|
	//  |---+---+---+---+---+---+---+---+-+-+-+-----+-+-+-+-+-+-+-+-+-+-|
	//
	// L and G bits enables each of the breakpoints respectively for local
	// thread only or all the threads (G is not working on Windows).
	//
	// Where "R/W" =
	//  00 -- Break on instruction execution only
	//  01 -- Break on data writes only
	//  10 -- undefined
	//  11 -- Break on data reads or writes but not instruction fetches
	//
	// And LEN =
	//  00 -- one-byte length
	//  01 -- two-byte length
	//  10 -- undefined
	//  11 -- four-byte length
	//
	// They define the behavior of how the processor will break when the
	// addresses in registers DR0 to DR3 are encountered.

	// Four hardware breakpoints are supported.
	roAssert(index >= 0 && index < 4);

	// Only sizes of 1, 2 or 4 bytes are supported.
	roAssert(size == 1 || size == 2 || size == 4);

	// Must be aligned on size.
	roAssert(IS_ALIGNED((roPtrInt)address, size));

	CONTEXT ctx;
	HANDLE threadHandle = GetCurrentThread();

	// The only registers we care about are the debug registers.
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

	// Get the thread context.
	if (!GetThreadContext(threadHandle, &ctx))
	{
		return false;
	}

	// Compute shift and mask for the R/W bits.
	int rwShift = index * 4 + 16;
	int rwMask = ~(3 << rwShift);

	// Compute shift and mask for the LEN bits.
	int lenShift = index * 4 + 18;
	int lenMask = ~(3 << lenShift);

	// Compute shift for the enable bits.
	int glShift = index * 2;

	roPtrInt rw = 1;	// Break on write.
	// rw |= 2;			// 11b = break on read-write.

	roPtrInt len = 3;	// Size == 4 by default.
	switch (size) {
	case 1: len = 0; break;
	case 2: len = 1; break;
	}

	switch (index) {
	case 0: ctx.Dr0 = (roPtrInt)address; break;
	case 1: ctx.Dr1 = (roPtrInt)address; break;
	case 2: ctx.Dr2 = (roPtrInt)address; break;
	case 3: ctx.Dr3 = (roPtrInt)address; break;
	}

	// Set R/W and LEN bits.
	ctx.Dr7 = (ctx.Dr7 & rwMask) | (rw << rwShift);
	ctx.Dr7 = (ctx.Dr7 & lenMask) | (len << lenShift);
	// Set global and local enable bits.
	ctx.Dr7 |= ((roPtrInt)1 << glShift);

	// Update the thread context.
	return (SetThreadContext(threadHandle, &ctx) == TRUE);
}

bool _roRemoveDataBreakpoint(unsigned index)
{
	// Four hardware breakpoints are supported.
	roAssert(index >= 0 && index < 4);

	CONTEXT ctx;
	HANDLE threadHandle = GetCurrentThread();

	// The only registers we care about are the debug registers.
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

	// Get the thread context.
	if(!GetThreadContext(threadHandle, &ctx))
		return false;

	// Compute shift for the enable bits.
	int glShift = index * 2;

	// Clear global and local enable bits.
	ctx.Dr7 &= ~(1 << glShift);

	// Update the thread context.
	return (SetThreadContext(threadHandle, &ctx) == TRUE);
}