#ifndef __roRawFileSystem_h__
#define __roRawFileSystem_h__

#include "roFileSystem.h"

namespace ro {

void*		rawFileSystemOpenFile		(const char* uri);
bool		rawFileSystemReadReady		(void* file, roUint64 size);
roUint64	rawFileSystemRead			(void* file, void* buffer, roUint64 size);
roUint64	rawFileSystemSize			(void* file);
int			rawFileSystemSeek			(void* file, roUint64 offset, FileSystem::SeekOrigin origin);
void		rawFileSystemCloseFile		(void* file);
roBytePtr	rawFileSystemGetBuffer		(void* file, roUint64 requestSize, roUint64& readableSize);
void		rawFileSystemTakeBuffer		(void* file);
void		rawFileSystemUntakeBuffer	(void* file, roBytePtr buf);
void*		rawFileSystemOpenDir		(const char* uri);
bool		rawFileSystemNextDir		(void* dir);
const char*	rawFileSystemDirName		(void* dir);
void		rawFileSystemCloseDir		(void* dir);

}	// namespace ro

#endif	// __roRawFileSystem_h__
