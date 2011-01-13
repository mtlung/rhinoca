#ifndef _TEST_HARNESS_H_
#define _TEST_HARNESS_H_

#ifdef _MSC_VER
#	ifdef NDEBUG
#		pragma comment(lib, "CppTestHarness")
#	else
#		pragma comment(lib, "CppTestHarnessd")
#	endif
#endif

#include "Test.h"
#include "TypedTestLauncher.h"
#include "TestResults.h"

#include "TestMacros.h"
#include "CheckMacros.h"

#include "TestRunner.h"


#endif	// _TEST_HARNESS_H_
