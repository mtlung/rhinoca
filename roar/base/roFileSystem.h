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
	bool		(*readWillBlock)(void* file, roUint64 bytesToRead);
	Status		(*read)			(void* file, void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
	Status		(*atomicRead)	(void* file, void* buffer, roUint64 bytesToRead);
	Status		(*size)			(void* file, roUint64& bytes);
	Status		(*seek)			(void* file, roInt64 offset, SeekOrigin origin);
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
extern bool uriNameMatch(const char* uri, const char* name, const char* extension);

}	// namespace ro

#endif	// __roFileSystem_h__
