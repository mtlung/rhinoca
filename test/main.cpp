#include "pch.h"
#include "../roar/base/roMemoryProfiler.h"
#include "../roar/base/roResource.h"
#include "../roar/base/roSocket.h"

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

//	ret = runner.RunTest("GraphicsDriverTest::textureUpdate");
//	ret = runner.RunTest("GraphicsDriverTest::font");
//	ret = runner.RunAllTests();

//	ret = runner.RunTest("FileSystemTest::httpFS_read");
//	ret = runner.RunTest("TextureLoaderTest::stressTest");
	ret = runner.RunTest("ImGuiTest::window");
//	ret = runner.RunTest("MatrixTest::transformVector");
//	ret = runner.RunTest("InputTest::mouse");
//	ret = runner.RunTest("CanvasTest::drawImage");
//	ret = runner.RunTest("AudioTest::soundSource");
//	ret = runner.RunTest("StringFormatTest::printf");
//	ret = runner.RunTest("AlgorithmTest::sort");

/*	using namespace ro;
	
	MemoryProfiler memoryProfiler;
	BsdSocket::initApplication();
	memoryProfiler.init(5000);

	malloc(123);

	while(true)
	{
		void* p = malloc(1);
		free(p);

		memoryProfiler.tick();
	}

	memoryProfiler.shutdown();
	BsdSocket::closeApplication();*/

	return int(ret);
}
