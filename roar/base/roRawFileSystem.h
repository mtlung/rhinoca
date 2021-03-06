#ifndef __roRawFileSystem_h__
#define __roRawFileSystem_h__

#include "roFileSystem.h"

namespace ro {

Status		rawFileSystemOpenFile		(const char* uri, void*& outFile);
bool		rawFileSystemReadWillBlock	(void* file, roUint64 bytesToRead);
Status		rawFileSystemRead			(void* file, void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
Status		rawFileSystemAtomicRead		(void* file, void* buffer, roUint64 bytesToRead);
Status		rawFileSystemSize			(void* file, roUint64& bytes);
Status		rawFileSystemSeek			(void* file, roInt64 offset, FileSystem::SeekOrigin origin);
void		rawFileSystemCloseFile		(void* file);
roBytePtr	rawFileSystemGetBuffer		(void* file, roUint64 requestSize, roUint64& readableSize);
void		rawFileSystemTakeBuffer		(void* file);
void		rawFileSystemUntakeBuffer	(void* file, roBytePtr buf);
void*		rawFileSystemOpenDir		(const char* uri);
bool		rawFileSystemNextDir		(void* dir);
const char*	rawFileSystemDirName		(void* dir);
void		rawFileSystemCloseDir		(void* dir);

extern FileSystem rawFileSystem;

}	// namespace ro

#endif	// __roRawFileSystem_h__
