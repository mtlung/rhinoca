#include "pch.h"
#include "roFileSystem.h"
#include "roHttpFileSystem.h"
#include "roRawFileSystem.h"
#include "roMemory.h"
#include "roStringUtility.h"

namespace ro {

static DefaultAllocator _allocator;

struct _CompoundFSContext
{
	void* impl;
	FileSystem* fsImpl;
};

void* _compoundFSOpenFile(const char* uri)
{
	if(!uri || uri[0] == '\0')
		return NULL;

	_CompoundFSContext ctx = { NULL, NULL };

	if(roStrStr(uri, "http://") == uri)
		ctx.fsImpl = &httpFileSystem;
	else
		ctx.fsImpl = &rawFileSystem;

	ctx.impl = ctx.fsImpl->openFile(uri);

	return ctx.impl ? _allocator.newObj<_CompoundFSContext>(ctx).unref() : NULL;
}

bool _compoundFSReadReady(void* file, roUint64 size)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->readReady(c->impl, size);
}

roUint64 _compoundFSRead(void* file, void* buffer, roUint64 size)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->read(c->impl, buffer, size);
}

roUint64 _compoundFSSize(void* file)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->size(c->impl);
}

int _compoundFSSeek(void* file, roUint64 offset, FileSystem::SeekOrigin origin)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->seek(c->impl, offset, origin);
}

void _compoundFSCloseFile(void* file)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	c->fsImpl->closeFile(c->impl);
	_allocator.deleteObj(c);
}

roBytePtr _compoundFSGetBuffer(void* file, roUint64 requestSize, roUint64& readableSize)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->getBuffer(c->impl, requestSize, readableSize);
}

void _compoundFSTakeBuffer(void* file)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->takeBuffer(c->impl);
}

void _compoundFSUntakeBuffer(void* file, roBytePtr buf)
{
	_CompoundFSContext* c = (_CompoundFSContext*)file;
	return c->fsImpl->untakeBuffer(c->impl, buf);
}


// ----------------------------------------------------------------------

void* _compoundFSOpenDir(const char* uri)
{
	if(!uri || uri[0] == '\0')
		return NULL;

	_CompoundFSContext ctx = { NULL, NULL };

	if(roStrStr(uri, "http://") == uri)
		ctx.fsImpl = &httpFileSystem;
	else
		ctx.fsImpl = &rawFileSystem;

	ctx.impl = ctx.fsImpl->openDir(uri);

	return ctx.impl ? _allocator.newObj<_CompoundFSContext>(ctx).unref() : NULL;
}

bool _compoundFSNextDir(void* dir)
{
	_CompoundFSContext* c = (_CompoundFSContext*)dir;
	return c->fsImpl->nextDir(c->impl);
}

const char* _compoundFSDirName(void* dir)
{
	_CompoundFSContext* c = (_CompoundFSContext*)dir;
	return c->fsImpl->dirName(c->impl);
}

void _compoundFSCloseDir(void* dir)
{
	_CompoundFSContext* c = (_CompoundFSContext*)dir;
	c->fsImpl->closeDir(c->impl);
	_allocator.deleteObj(c);
}


// ----------------------------------------------------------------------

static FileSystem _compoundFS = {
	_compoundFSOpenFile,
	_compoundFSReadReady,
	_compoundFSRead,
	_compoundFSSize,
	_compoundFSSeek,
	_compoundFSCloseFile,
	_compoundFSGetBuffer,
	_compoundFSTakeBuffer,
	_compoundFSUntakeBuffer,
	_compoundFSOpenDir,
	_compoundFSNextDir,
	_compoundFSDirName,
	_compoundFSCloseDir
};

FileSystem fileSystem = _compoundFS;

bool uriExtensionMatch(const char* uri, const char* extension)
{
	roSize uriLen = roStrLen(uri);
	roSize extLen = roStrLen(extension);

	if(uriLen < extLen) return false;
	return roStrCaseCmp(uri + uriLen - extLen, extension) == 0;
}

}	// namespace ro
