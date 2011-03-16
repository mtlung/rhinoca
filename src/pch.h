// Reference:
// https://developer.mozilla.org/en/SpiderMonkey/JSAPI_Reference

#ifdef _MSC_VER

#define _WINSOCKAPI	// stops windows.h including winsock.h
#include <windows.h>
#undef _WINSOCKAPI_
#include <gl/gl.h>

#define XP_WIN
#include <assert.h>
#include <stdio.h>
#ifdef _DEBUG
#	define DEBUG
#endif
#include "../thirdParty/SpiderMonkey/jsapi.h"
#undef XP_WIN

#endif	// _MSC_VER