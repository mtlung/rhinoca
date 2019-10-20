#include "pch.h"
#include "../roar/base/roMemoryProfiler.h"
#include "../roar/base/roResource.h"
#include "../roar/network/roSocket.h"
#include <crtdbg.h>

int main()
{
#ifdef _MSC_VER
	// Tell the c-run time to do memory check at program shut down
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetBreakAlloc(-1);
#endif

	size_t ret;
	ro::cpptest::TestRunner runner;
	runner.showTestName(true);

//	ret = runner.runTest("GraphicsDriverTest::textureUpdate");
//	ret = runner.runTest("GraphicsDriverTest::font");
//	ret = runner.RunAllTests();

//	ret = runner.runTest("FileSystemTest::httpFS_read");
//	ret = runner.runTest("TextureLoaderTest::stressTest");
	ret = runner.runTest("ImGuiTest::window");
//	ret = runner.runTest("MatrixTest::transformVector");
//	ret = runner.runTest("InputTest::mouse");
//	ret = runner.runTest("CanvasTest::drawImage");
//	ret = runner.runTest("AudioTest::soundSource");
//	ret = runner.runTest("StringFormatTest::printf");
//	ret = runner.runTest("AlgorithmTest::sort");

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
