#ifndef __roPlatformHeaders_h__
#define __roPlatformHeaders_h__

#include "roCompiler.h"
#include "roOS.h"

#if roOS_WIN
// Define the following to exclude some unused services from the windows headers
#	define NOGDICAPMASKS
#	define NOMENUS
#	define NOATOM
//#	define NODRAWTEXT
#	define NOKERNEL
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX
#	define NOCRYPT

// Define these for MFC projects
#	define NOTAPE
#	define NOIMAGE
#	define NOPROXYSTUB
#	define NORPC

#	define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#	define WIN32_LEAN_AND_MEAN

#	include <windows.h>
#endif

#endif	// __roPlatformHeaders_h__
