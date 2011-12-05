#include "pch.h"
#include "roFileSystem.h"
#include "roArray.h"
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
	roSize bufSize;
};

void* rawFileSystemOpenFile(const char* uri)
{
	if(!uri) return NULL;
	_RawFile ret = { fopen(uri, "rb"), NULL, 0 };
	if(!ret.file) return NULL;
	return _allocator.newObj<_RawFile>(ret);
}

bool rawFileSystemReadReady(void* file, roUint64 size)
{
	roAssert(file); if(!file) return false;
	return true;
}

roUint64 rawFileSystemRead(void* file, void* buffer, roUint64 size)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;
	return num_cast<roUint64>(fread(buffer, 1, clamp_cast<size_t>(size), impl->file));
}

roUint64 rawFileSystemSize(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;
	struct stat st;
	if(fstat(fileno(impl->file), &st) != 0)
		return roUint64(-1);
	return st.st_size;
}

int rawFileSystemSeek(void* file, roUint64 offset, FileSystem::SeekOrigin origin)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;
	return fseek(impl->file, num_cast<long>(offset), origin) == 0 ? 1 : 0;
}

void rawFileSystemCloseFile(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return;
	fclose(impl->file);
	_allocator.free(impl->buf);
	_allocator.deleteObj(impl);
}

roBytePtr rawFileSystemGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	_RawFile* impl = (_RawFile*)(file);
	if(!impl) { readableSize = 0; return NULL; }
	const roSize rqSize = clamp_cast<roSize>(requestSize);
	impl->buf = _allocator.realloc(impl->buf, impl->bufSize, rqSize);
	impl->bufSize = rqSize;
	readableSize = rawFileSystemRead(file, impl->buf, rqSize);
	return impl->buf;
}

void rawFileSystemTakeBuffer(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	if(!impl) return;
	impl->buf = NULL;
	impl->bufSize = 0;
}

void rawFileSystemUntakeBuffer(void* file, roBytePtr buf)
{
	_allocator.free(buf);
}

}	// namespace ro
