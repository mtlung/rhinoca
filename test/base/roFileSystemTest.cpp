#include "pch.h"
#include "../../roar/base/roFileSystem.h"
#include "../../roar/base/roStringUtility.h"

using namespace ro;

struct FileSystemTest {};

TEST_FIXTURE(FileSystemTest, rawFS_getBuffer)
{
	void* file = rawFileSystemOpenFile("Non-existing file");
	CHECK(!file);

	file = rawFileSystemOpenFile("Test.vc9.vcproj");
	if(!file)
		return;

	roUint64 size;
	char* buf1 = rawFileSystemGetBuffer(file, 8148, size);
	rawFileSystemTakeBuffer(file);

	char* buf2 = rawFileSystemGetBuffer(file, 8148, size);
	rawFileSystemTakeBuffer(file);

	rawFileSystemSeek(file, 0, FileSystem::SeekOrigin_End);

	char* buf3 = rawFileSystemGetBuffer(file, 8148, size);
	CHECK(buf3 != NULL);
	CHECK_EQUAL(0u, size);

	rawFileSystemUntakeBuffer(file, buf1);
	rawFileSystemUntakeBuffer(file, buf2);

	rawFileSystemCloseFile(file);
}

TEST_FIXTURE(FileSystemTest, rawFS_directoryListing)
{
	void* dir = rawFileSystemOpenDir("./");

	while(dir) {
		const char* name = rawFileSystemDirName(dir);

		CHECK(roStrLen(name) > 0);

		if(!rawFileSystemNextDir(dir))
			break;
	}

	rawFileSystemCloseDir(dir);

	CHECK(true);
}
