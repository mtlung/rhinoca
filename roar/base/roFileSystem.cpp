#include "pch.h"
#include "roFileSystem.h"
#include "roMemory.h"
#include "roTypeCast.h"
#include <stdio.h>
#include <sys/stat.h>

namespace ro {

static roDefaultAllocator _allocator;

FileSystem*	defaultFileSystem();
void setDefaultFileSystem(FileSystem* fs);

struct _RawFile {
	FILE* file;
	roBytePtr buf;
};

void* rawFileSystemOpenFile(const char* uri)
{
	_RawFile ret = { fopen(uri, "rb"), NULL };
	return _allocator.newObj<_RawFile>(ret);
}

bool rawFileSystemReadReady(void* file, roUint64 size)
{
	return true;
}

roUint64 rawFileSystemRead(void* file, void* buffer, roUint64 size)
{
	_RawFile* impl = (_RawFile*)(file);
	return num_cast<roUint64>(fread(buffer, 1, num_cast<size_t>(size), impl->file));
}

roUint64 rawFileSystemSize(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	struct stat st;
	if(fstat(fileno(impl->file), &st) != 0)
		return roUint64(-1);
	return st.st_size;
}

int rawFileSystemSizeSeek(void* file, roUint64 offset, FileSystem::SeekOrigin origin)
{
	_RawFile* impl = (_RawFile*)(file);
	return fseek(impl->file, num_cast<long>(offset), origin) == 0 ? 1 : 0;
}

void rawFileSystemSizeSeekCloseFile(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	fclose(impl->file);
	_allocator.free(impl->buf);
	_allocator.deleteObj(impl);
}

roBytePtr rawFileSystemGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	return NULL;
}

roUint64 rawFileSystemTakeBuffer(void* file)
{
	return 0;
}

void rawFileSystemUntakeBuffer(void* file, roBytePtr buf)
{
}

}	// namespace ro
