#ifndef __roOS_h__
#define __roOS_h__

#include "roCompiler.h"

#if   roOS_WIN32   + roOS_WIN64 + roOS_WINCE \
	+ roOS_FreeBSD + roOS_Linux + roOS_Solaris \
	+ roOS_MacOSX  + roOS_iOS + roOS_MinGW != 1
#	error OS should be specified
#endif

#if roOS_WIN32 || roOS_WIN64 || roOS_WINCE || roOS_MinGW
#	define roOS_WIN			1
#	include "roOS.win.h"
#endif

#if roOS_Linux
#	include "roOS.linux.h"
#endif

#if roOS_iOS
#	include "roOS.iOS.h"
#endif

#if roOS_MacOSX
#	include "roOS.mac.h"
#endif

#if roOS_FreeBSD
#	include "roOS.freebsd.h"
#endif

#if roOS_FreeBSD || roOS_Linux || roOS_Solaris || roOS_MacOSX || roOS_iOS
#	define roOS_UNIX		1
#	define roUSE_PTHREAD	1
#	include "roOS.unix.h"
#endif

#endif	// __roOS_h__
