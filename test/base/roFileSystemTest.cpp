#include "pch.h"
#include "../../roar/base/roRawFileSystem.h"
#include "../../roar/base/roHttpFileSystem.h"
#include "../../roar/base/roStringUtility.h"
#include "../../roar/base/roString.h"

using namespace ro;

struct FileSystemTest {};

TEST_FIXTURE(FileSystemTest, defaultFS)
{
	void* file = NULL;
	Status st = fileSystem.openFile("Test.vc9.vcproj", file);
	CHECK(st);
	CHECK(file);

	roUint64 size = fileSystem.size(file);
	CHECK(size > 0);

	fileSystem.closeFile(file);
}

TEST_FIXTURE(FileSystemTest, rawFS_getBuffer)
{
	void* file = NULL;
	Status st = rawFileSystemOpenFile("Non-existing file", file);
	CHECK(!st);
	CHECK(!file);

	st = rawFileSystemOpenFile("Test.vc9.vcproj", file);
	if(!st)
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

TEST_FIXTURE(FileSystemTest, httpFS_read)
{
//	return;	// Enable when needed

	void* file = NULL;
	Status st = httpFileSystemOpenFile("http://www.google.com/", file);

	roUint64 size = httpFileSystemSize(file);
	CHECK(size > 0);

	char buf[128];
	roUint64 readCount = httpFileSystemRead(file, buf, sizeof(buf));
	CHECK(readCount > 0);

	readCount = httpFileSystemRead(file, buf, sizeof(buf));
	readCount = httpFileSystemRead(file, buf, sizeof(buf));

	httpFileSystemCloseFile(file);
}

TEST_FIXTURE(FileSystemTest, httpFS_getBuffer)
{
//	return;	// Enable when needed

	void* file = NULL;
	Status st = httpFileSystemOpenFile("http://www.cplusplus.com/", file);

	roUint64 readCount;
	roBytePtr p1 = httpFileSystemGetBuffer(file, httpFileSystemSize(file) / 2, readCount);

	httpFileSystemTakeBuffer(file);

	roBytePtr p2 = httpFileSystemGetBuffer(file, httpFileSystemSize(file) / 2, readCount);

	char buf[1024];
	readCount = httpFileSystemRead(file, buf, sizeof(buf));

	httpFileSystemUntakeBuffer(file, p1);
	httpFileSystemCloseFile(file);
}
