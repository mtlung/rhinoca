// Reference:
// https://developer.mozilla.org/en/SpiderMonkey/JSAPI_Reference
#define XP_WIN
#include <assert.h>
#include <stdio.h>
#ifdef _DEBUG
#	define DEBUG
#endif
#include "../thirdParty/SpiderMonkey/jsapi.h"
#undef XP_WIN
