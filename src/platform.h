#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "rhinoca.h"

#if defined(RHINOCA_WINDOWS)
#	define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#	define WIN32_LEAN_AND_MEAN

#	include <windows.h>
#endif

#endif
