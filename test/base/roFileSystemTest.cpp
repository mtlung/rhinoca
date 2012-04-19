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

	roUint64 size = 0;
	CHECK(fileSystem.size(file, size));
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
//	Status st = httpFileSystemOpenFile("http://www.gamearchitect.net/Images/bigben.gif", file);	// No chunked, know size
	Status st = httpFileSystemOpenFile("http://yahoo.com", file);

	roUint64 size = 0;
	do {
		st = httpFileSystemSize(file, size);
	} while(st == Status::in_progress);
	CHECK(st);
	printf(st.c_str());
	CHECK(size > 0);

	roUint64 bytesRead = 0;
//	char buf[128888];
	String buf;
	buf.resize(10 * 1024);
	CHECK(httpFileSystemRead(file, buf.c_str(), buf.size(), bytesRead));
	CHECK(bytesRead > 0);

	do {
		CHECK(httpFileSystemRead(file, buf.c_str(), buf.size(), bytesRead));

		if(bytesRead < buf.size())
			buf[bytesRead] = 0;
	} while(bytesRead > 0);
//	readCount = httpFileSystemRead(file, buf, sizeof(buf));

	httpFileSystemCloseFile(file);
}

TEST_FIXTURE(FileSystemTest, httpFS_getBuffer)
{
//	return;	// Enable when needed

	void* file = NULL;
	Status st = httpFileSystemOpenFile("http://www.cplusplus.com/", file);

	roUint64 size = 0;
	CHECK(httpFileSystemSize(file, size));

	roUint64 readCount;
	roBytePtr p1 = httpFileSystemGetBuffer(file, size / 2, readCount);

	httpFileSystemTakeBuffer(file);

	roBytePtr p2 = httpFileSystemGetBuffer(file, size, readCount);

	char buf[1024];
	CHECK(httpFileSystemRead(file, buf, sizeof(buf), readCount));

	httpFileSystemUntakeBuffer(file, p1);
	httpFileSystemCloseFile(file);
}
