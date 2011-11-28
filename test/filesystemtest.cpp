#include "pch.h"
#include "../src/rhinoca.h"
#include "../src/common.h"
#include "../src/path.h"

class FileSystemTest
{
public:
};

TEST_FIXTURE(FileSystemTest, directoryListing)
{
	Path path("./");
	void* dir = rhFileSystem.openDir(NULL, path.c_str());

	while(dir) {
		const char* name = rhFileSystem.dirName(dir);

		CHECK(strlen(name) > 0);

		if(!rhFileSystem.nextDir(dir))
			break;
	}

	rhFileSystem.closeDir(dir);

	CHECK(true);
}
