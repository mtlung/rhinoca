// Reference:
// https://developer.mozilla.org/en/SpiderMonkey/JSAPI_Reference

#ifdef _MSC_VER

#include "platform.h"

#define XP_WIN
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#ifdef _DEBUG
#	define DEBUG
#endif
//#include "../thirdParty/SpiderMonkey/jsapi.h"
#undef XP_WIN

#endif	// _MSC_VER