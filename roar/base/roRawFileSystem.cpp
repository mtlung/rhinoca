#include "pch.h"
#include "roRawFileSystem.h"
#include "roArray.h"
#include "roString.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include <stdio.h>
#include <sys/stat.h>

namespace ro {

static DefaultAllocator _allocator;

FileSystem*	defaultFileSystem();
void setDefaultFileSystem(FileSystem* fs);

#if roOS_WIN

struct _RawFile {
	HANDLE file;
	bool readInProgress;
	OVERLAPPED overlap;
	roBytePtr buf;
	roSize bufOffset;
	roSize bufSize;
	roSize readable;
};

void* rawFileSystemOpenFile(const char* uri)
{
	if(!uri) return NULL;

	Array<roUint16> wstr;
	{	roSize len = 0;
		if(!roUtf8ToUtf16(NULL, len, uri, roSize(-1))) return NULL;
		if(!wstr.resize(len+1)) return NULL;
		if(!roUtf8ToUtf16(wstr.typedPtr(), len, uri, roSize(-1))) return NULL;
		wstr[len] = 0;
	}

	HANDLE h = CreateFileW((wchar_t*)wstr.typedPtr(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if(h == INVALID_HANDLE_VALUE)
		return NULL;

	_RawFile ret = { h, false, {0}, NULL, 0, 0, 0 };
	return _allocator.newObj<_RawFile>(ret).unref();
}

bool rawFileSystemReadReady(void* file, roUint64 size)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return false;

CheckProgress:
	if(impl->readInProgress) {
		DWORD transferred = 0;
		if(!GetOverlappedResult(impl->file, &impl->overlap, &transferred, false)) {
			if(GetLastError() == ERROR_IO_PENDING)
				return false;

			// If EOF or error occurred
			return true;
		}

		impl->overlap.Offset += transferred;
		impl->readInProgress = false;
		impl->bufOffset = impl->readable;
		impl->readable += transferred;
		return true;
	}

	DWORD bytesToRead = clamp_cast<unsigned int>(size);
	roSize bufSize = roMaxOf2(impl->readable + (roSize)bytesToRead, (roSize)1024);
	if(bufSize > impl->bufSize) {
		impl->buf = _allocator.realloc(impl->buf, impl->bufSize, bufSize);
		impl->bufSize = bufSize;
	}

	if(::ReadFile(impl->file, impl->buf + impl->readable, bytesToRead, NULL, &impl->overlap)) {
		impl->readInProgress = true;
		goto CheckProgress;
	}

	if(GetLastError() == ERROR_IO_PENDING) {
		impl->readInProgress = true;
		goto CheckProgress;
	}

	// If error occurred
	impl->readable = 0;
	return true;
}

roUint64 rawFileSystemRead(void* file, void* buffer, roUint64 size)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;

	while(impl->readable < size && !rawFileSystemReadReady(file, size - impl->readable)) {}

	// NOTE: A memory copy overhead here
	roSize bytesToMove = roMinOf2(clamp_cast<roSize>(size), impl->readable);
	roAssert(impl->readable >= bytesToMove);
	roMemcpy(buffer, impl->buf + impl->bufOffset, bytesToMove);

	impl->bufOffset += bytesToMove;
	impl->readable -= bytesToMove;

	return bytesToMove;
}

roUint64 rawFileSystemSize(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;

	LARGE_INTEGER fileSize;
	if(!GetFileSizeEx(impl->file, &fileSize))
		return 0;
	return fileSize.QuadPart;
}

int	rawFileSystemSeek(void* file, roUint64 offset, FileSystem::SeekOrigin origin)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return 0;

	LARGE_INTEGER absOffset;
	if(origin == FileSystem::SeekOrigin_Begin)
		absOffset.QuadPart = offset;
	else if(origin == FileSystem::SeekOrigin_Current) {
		absOffset.LowPart = impl->overlap.Offset;
		absOffset.HighPart = impl->overlap.OffsetHigh;
		absOffset.QuadPart += offset;
	}
	else if(origin == FileSystem::SeekOrigin_End) {
		if(!GetFileSizeEx(impl->file, &absOffset))
			return 0;
		absOffset.QuadPart -= offset;
	}
	else {
		roAssert(false);
		return 0;
	}

	impl->overlap.Offset = absOffset.LowPart;
	impl->overlap.OffsetHigh = absOffset.HighPart;
	return 1;
}

void rawFileSystemCloseFile(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	roAssert(impl); if(!impl) return;
	roVerify(CloseHandle(impl->file));
	_allocator.free(impl->buf);
	_allocator.deleteObj(impl);
}

roBytePtr rawFileSystemGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	_RawFile* impl = (_RawFile*)(file);
	if(!impl) { readableSize = 0; return NULL; }

	while(impl->readable < requestSize && !rawFileSystemReadReady(file, requestSize - impl->readable)) {}

	readableSize = roMinOf2(num_cast<roSize>(requestSize), impl->readable);
	impl->bufOffset += impl->readable;
	impl->readable -= clamp_cast<roSize>(readableSize);

	return impl->buf;
}

void rawFileSystemTakeBuffer(void* file)
{
	_RawFile* impl = (_RawFile*)(file);
	if(!impl) return;

	// The whole buffer can give to the client
	if(impl->readable == 0) {
		impl->buf = NULL;
		impl->bufSize = 0;
		impl->bufOffset = 0;
	}
	// Else we need to keep part of the buffer than haven't read by the client
	else {
		roBytePtr oldBuf = impl->buf;
		impl->buf = _allocator.malloc(impl->bufSize);
		roMemcpy(impl->buf, oldBuf + impl->bufOffset, impl->readable);
		impl->bufOffset = 0;
	}
}

void rawFileSystemUntakeBuffer(void* file, roBytePtr buf)
{
	_allocator.free(buf);
}

#else

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
	return num_cast<roUint64>(st.st_size);
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

#endif


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


// ----------------------------------------------------------------------

FileSystem rawFileSystem = {
	rawFileSystemOpenFile,
	rawFileSystemReadReady,
	rawFileSystemRead,
	rawFileSystemSize,
	rawFileSystemSeek,
	rawFileSystemCloseFile,
	rawFileSystemGetBuffer,
	rawFileSystemTakeBuffer,
	rawFileSystemUntakeBuffer,
	rawFileSystemOpenDir,
	rawFileSystemNextDir,
	rawFileSystemDirName,
	rawFileSystemCloseDir
};

}	// namespace ro
