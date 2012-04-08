#ifndef __roFileSystem_h__
#define __roFileSystem_h__

#include "roBytePtr.h"
#include "roStatus.h"

namespace ro {

struct FileSystem
{
	enum SeekOrigin {
		SeekOrigin_Begin	= 0,
		SeekOrigin_Current	= 1,
		SeekOrigin_End		= 2
	};

// File read operations
	Status		(*openFile)		(const char* uri, void*& outFile);
	bool		(*readReady)	(void* file, roUint64 size);
	roUint64	(*read)			(void* file, void* buffer, roUint64 size);
	roUint64	(*size)			(void* file);										///< Returns roUint64(-1) if the file size is unknown.
	int			(*seek)			(void* file, roUint64 offset, SeekOrigin origin);	///< Returns 1 for success, 0 for fail, -1 not supported.
	void		(*closeFile)	(void* file);

	roBytePtr	(*getBuffer)	(void* file, roUint64 requestSize, roUint64& readableSizeLEqRequest);	///< If fail, readableSize (<= requestSize) will be zero but may/may-not return NULL.
	void		(*takeBuffer)	(void* file);					///< Take over the ownership of the buffer returned by last getBuffer(), in this case it's recommended to not call readReady() with excessive size.
	void		(*untakeBuffer)	(void* file, roBytePtr buf);	///< Give the buffer returned by getBuffer() back to FileSystem to handle.

// Directory operations
	void*		(*openDir)		(const char* uri);
	bool		(*nextDir)		(void* dir);
	const char*	(*dirName)		(void* dir);	///< The string returned is managed by the directory context, no need to free by user.
	void		(*closeDir)		(void* dir);
};

extern FileSystem fileSystem;

extern bool uriExtensionMatch(const char* uri, const char* extension);

}	// namespace ro

#endif	// __roFileSystem_h__
