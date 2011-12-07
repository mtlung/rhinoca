#include "pch.h"
#include "roFileSystem.h"
#include "roArray.h"
#include "roMemory.h"
#include "roString.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "../platform/roOS.h"
#include "../platform/roPlatformHeaders.h"
#include <stdio.h>
#include <sys/stat.h>

namespace ro {

static DefaultAllocator _allocator;

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
	return _allocator.newObj<_RawFile>(ret).unref();
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


// ----------------------------------------------------------------------

#if roOS_WIN

struct OpenDirContext
{
	~OpenDirContext() {
		if(handle != INVALID_HANDLE_VALUE)
			::FindClose(handle);
	}

	HANDLE handle;
	WIN32_FIND_DATAW data;

	String str;
};	// OpenDirContext

void* rawFileSystemOpenDir(const char* uri)
{
	if(roStrLen(uri) == 0) return NULL;

	Array<wchar_t> buf;
	{	// Convert the source uri to utf 16
		roSize len = 0;
		if(!roUtf8ToUtf16(NULL, len, uri, roSize(-1)))
			return NULL;

		buf.resize(len);
		roStaticAssert(sizeof(wchar_t) == sizeof(roUint16));
		if(len == 0 || !roUtf8ToUtf16((roUint16*)&buf[0], len, uri, roSize(-1)))
			return NULL;
	}

	AutoPtr<OpenDirContext> dirCtx = _allocator.newObj<OpenDirContext>();

	// Add wild-card at the end of the path
	if(buf.back() != L'/' && buf.back() != L'\\')
		buf.pushBack(L'/');
	buf.pushBack(L'*');
	buf.pushBack(L'\0');

	HANDLE h = ::FindFirstFileW(&buf[0], &(dirCtx->data));

	// Skip the ./ and ../
	while(::wcscmp(dirCtx->data.cFileName, L".") == 0 || ::wcscmp(dirCtx->data.cFileName, L"..") == 0) {
		if(!FindNextFileW(h, &(dirCtx->data))) {
			dirCtx->data.cFileName[0] = L'\0';
			break;
		}
	}

	if(h != INVALID_HANDLE_VALUE)
		dirCtx->handle = h;
	else
		dirCtx.deleteObject();

	return dirCtx.unref();
}

bool rawFileSystemNextDir(void* dir)
{
	OpenDirContext* dirCtx = reinterpret_cast<OpenDirContext*>(dir);
	if(!dirCtx) return false;

	if(!::FindNextFileW(dirCtx->handle, &(dirCtx->data))) {
		dirCtx->str.clear();
		return false;
	}

	return true;
}

const char* rawFileSystemDirName(void* dir)
{
	OpenDirContext* dirCtx = reinterpret_cast<OpenDirContext*>(dir);
	if(!dirCtx) return false;

	if(!dirCtx->str.fromUtf16((roUint16*)dirCtx->data.cFileName, roSize(-1)))
		dirCtx->str.clear();

	return dirCtx->str.c_str();
}

void rawFileSystemCloseDir(void* dir)
{
	OpenDirContext* dirCtx = reinterpret_cast<OpenDirContext*>(dir);
	_allocator.deleteObj(dirCtx);
}

#else

void* rawFileSystemOpenDir(const char* uri)
{
	return NULL;
}

bool rawFileSystemNextDir(void* dir)
{
	return false;
}

const char* rawFileSystemDirName(void* dir)
{
	return NULL;
}

void rawFileSystemCloseDir(void* dir)
{
}

#endif

}	// namespace ro
