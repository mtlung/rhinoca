#ifndef __roHttpFileSystem_h__
#define __roHttpFileSystem_h__

#include "roFileSystem.h"

namespace ro {

Status		httpFileSystemOpenFile		(const char* uri, void*& outFile);
bool		httpFileSystemReadWillBlock	(void* file, roUint64 bytesToRead);
Status		httpFileSystemRead			(void* file, void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
Status		httpFileSystemAtomicRead	(void* file, void* buffer, roUint64 bytesToRead);
Status		httpFileSystemSize			(void* file, roUint64& bytes);
Status		httpFileSystemSeek			(void* file, roInt64 offset, FileSystem::SeekOrigin origin);
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
