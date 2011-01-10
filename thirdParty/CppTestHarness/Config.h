#ifndef CppTestHarness_Config_H
#define CppTestHarness_Config_H

#ifdef _MSC_VER
#	define VISUAL_STUDIO
#	if _MSC_VER == 1310
#		define VISUAL_STUDIO_2003
#	elif _MSC_VER == 1400
#		define VISUAL_STUDIO_2005
#	endif
#else
#include <bits/stringfwd.h>
namespace std {
	// Cygwin missing the typedef for std::wstring
	typedef basic_string<wchar_t> wstring;
}

#endif

#endif
