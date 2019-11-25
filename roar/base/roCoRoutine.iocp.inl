#pragma once
#include "roCoRoutine.h"
#include "../platform/roPlatformHeaders.h"

namespace ro {

struct ExtendedOverlapped : public OVERLAPPED
{
	ExtendedOverlapped() { roZeroMemory(this, sizeof(*this)); }
	Coroutine* coroutine;
};	// ExtendedOverlapped

}	// namespace ro
