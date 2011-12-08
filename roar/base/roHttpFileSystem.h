#ifndef __roHttpFileSystem_h__
#define __roHttpFileSystem_h__

#include "roFileSystem.h"

namespace ro {

void*		httpFileSystemOpenFile		(const char* uri);
bool		httpFileSystemReadReady		(void* file, roUint64 size);
roUint64	httpFileSystemRead			(void* file, void* buffer, roUint64 size);
roUint64	httpFileSystemSize			(void* file);
int			httpFileSystemSeek			(void* file, roUint64 offset, FileSystem::SeekOrigin origin);
void		httpFileSystemCloseFile		(void* file);
roBytePtr	httpFileSystemGetBuffer		(void* file, roUint64 requestSize, roUint64& readableSize);
void		httpFileSystemTakeBuffer	(void* file);
void		httpFileSystemUntakeBuffer	(void* file, roBytePtr buf);
void*		httpFileSystemOpenDir		(const char* uri);
bool		httpFileSystemNextDir		(void* dir);
const char*	httpFileSystemDirName		(void* dir);
void		httpFileSystemCloseDir		(void* dir);

extern FileSystem httpFileSystem;

}	// namespace ro

#endif	// __roHttpFileSystem_h__
