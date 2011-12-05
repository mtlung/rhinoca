#ifndef __roFileSystem_h__
#define __roFileSystem_h__

#include "roBytePtr.h"

namespace ro {

struct FileSystem
{
	enum SeekOrigin {
		SeekOrigin_Begin = 0,
		SeekOrigin_Current = 1,
		SeekOrigin_End = 2
	};

// File read operations
	void*		(*openFile)		(const char* uri);
	bool		(*readReady)	(void* file, roUint64 size);
	roUint64	(*read)			(void* file, void* buffer, roUint64 size);
	roUint64	(*size)			(void* file);										///< Returns roUint64(-1) if the file size is unknown.
	int			(*seek)			(void* file, roUint64 offset, SeekOrigin origin);	///< Returns 1 for success, 0 for fail, -1 not supported.
	void		(*closeFile)	(void* file);

	roBytePtr	(*getBuffer)	(void* file, roUint64 requestSize, roUint64& readableSize);	///< Always return non-null, but readableSize may be zero.
	void		(*takeBuffer)	(void* file);					///< Take over the ownership of the buffer returned by last getBuffer()
	void		(*untakeBuffer)	(void* file, roBytePtr buf);	///< Give the buffer returned by getBuffer() back to FileSystem to handle.

// Directory operations
	void*		(*openDir)		(const char* uri);
	bool		(*nextDir)		(void* dir);
	const char*	(*dirName)		(void* dir);	///< The string returned is managed by the directory context, no need to free by user.
	void		(*closeDir)		(void* dir);
};

FileSystem*	defaultFileSystem();
void		setDefaultFileSystem(FileSystem* fs);


// ----------------------------------------------------------------------

void*		rawFileSystemOpenFile		(const char* uri);
bool		rawFileSystemReadReady		(void* file, roUint64 size);
roUint64	rawFileSystemRead			(void* file, void* buffer, roUint64 size);
roUint64	rawFileSystemSize			(void* file);
int			rawFileSystemSeek			(void* file, roUint64 offset, FileSystem::SeekOrigin origin);
void		rawFileSystemCloseFile		(void* file);
roBytePtr	rawFileSystemGetBuffer		(void* file, roUint64 requestSize, roUint64& readableSize);
void		rawFileSystemTakeBuffer		(void* file);
void		rawFileSystemUntakeBuffer	(void* file, roBytePtr buf);

}	// namespace ro

#endif	// __roFileSystem_h__
