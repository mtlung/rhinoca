#define AL_NO_PROTOTYPES
#define ALC_NO_PROTOTYPES

#include "../../thirdparty/OpenAL/al.h"
#include "../../thirdparty/OpenAL/alc.h"
#define _(DECLAR, NAME) DECLAR NAME = NULL;
#	include "roAudioDriver.openal.windows.functionList.h"
#undef _

#include "../platform/roPlatformHeaders.h"

HMODULE _hOpenAL = 0;

// Load OpenAL dynamically
// I need to do so because the support of OpenAL on 64-bits was bad.
// Reference: 
// https://maxmods.googlecode.com/svn-history/r1237/trunk/theoraplayeropenal.mod/audio_interface/src/OpenAL_AudioInterface.cpp
// Also contains code for loading OpenAL on iOS dynamically
bool _initOpenAL()
{
	if(_hOpenAL)
		return false;

	if(!_hOpenAL) _hOpenAL = ::LoadLibraryA("OpenAL.dll");
	if(!_hOpenAL) _hOpenAL = ::LoadLibraryA("OpenAL32.dll");
	if(!_hOpenAL) _hOpenAL = ::LoadLibraryA("OpenAL64.dll");

	#define GET_FUNC_PTR(type, ptr) \
	if((ptr = (type) ::GetProcAddress(_hOpenAL, #ptr)) == NULL) return false;

	#define _(DECLAR, NAME) GET_FUNC_PTR(DECLAR, NAME);
	#	include "roAudioDriver.openal.windows.functionList.h"
	#undef _

	#undef GET_FUNC_PTR

	return true;
}

void _unloadOpenAL()
{
	if(_hOpenAL)
		roVerify(::FreeLibrary(_hOpenAL) > 0);
	_hOpenAL = 0;
}
