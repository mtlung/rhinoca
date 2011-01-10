#include "pch.h"

int main()
{
#ifdef _MSC_VER
	// Tell the c-run time to do memory check at program shut down
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetBreakAlloc(-1);
#endif

	size_t ret;
	CppTestHarness::TestRunner runner;
	runner.ShowTestName(true);

	ret = runner.RunAllTests();

	return int(ret);
}
