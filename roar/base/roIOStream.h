#ifndef __roIOStream_h__
#define __roIOStream_h__

#include "roArray.h"
#include "roMemory.h"

namespace ro {

// Reference: http://fgiesen.wordpress.com/2011/11/21/buffer-centric-io/
struct IStream : private NonCopyable
{
	enum SeekOrigin {
		SeekOrigin_Begin	= 0,
		SeekOrigin_Current	= 1,
		SeekOrigin_End		= 2
	};

			IStream();
	virtual ~IStream() { closeRead(); }

// Core interface that must be implement

	// Check if reading this amount of data will cause other read functions to block.
	// It also trigger a read request so async read would begin in the background.
	// Note that it will return false if there was NOT enough data or any error occurred.
	virtual bool		readWillBlock	(roUint64 minBytesToRead) { return false; }

	virtual Status		size			(roUint64& bytes) const { return roStatus::not_supported; }
	virtual roUint64	posRead			() const { return 0; }
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin) { return roStatus::not_supported; }
	virtual Status		closeRead		() { return roStatus::ok; }

// Standard helper functions, may consider make it virtual to maximize performance

	// Try to read the requested amount of data, and report how much was actually read.
	virtual	Status		read			(void* buffer, roUint64 maxBytesToRead, roUint64& actualBytesRead);

	// Typed read function
	template<class T>
			Status		read			(T& val) { return atomicRead(&val, sizeof(T)); }

	// Try to read the requested amount of data, return failure (and nothing is touched) if not enough data.
			Status		atomicRead		(void* buffer, roUint64 bytesToRead);

	// Get pointer to buffer that ready to read.
			Status		peek			(roByte*& outBuf, roSize& outBytesReady);

	// Ensure the requested amount of data will be available contiguously.
	// It may report size that is bigger than you requested.
			Status		atomicPeek		(roByte*& outBuf, roUint64& inoutMinBytesToPeek);

			Status		skip			(roUint64 bytes);

// Attributes
	roByte*	begin;
	roByte*	current;
	roByte*	end;
	mutable Status st;

	// Concrete class should implement this function to adjust the buffer pointers
	// to fulfill the read size requested as close as possible.
	// Client could keep calling this function to get the desired readable size.
	// Why make it a function pointer instead of a virtual function callback?
	// because this way we can construct a state machine inside the stream easily.
	Status (*_next)(IStream& s, roUint64 suggestedBytesToRead);
};	// IStream

struct OStream : private NonCopyable
{
	enum SeekOrigin {
		SeekOrigin_Begin	= 0,
		SeekOrigin_Current	= 1,
		SeekOrigin_End		= 2
	};

	virtual ~OStream() {}
	virtual Status		seekWrite		(roInt64 offset, SeekOrigin origin) { return roStatus::not_supported; }
	virtual Status		write			(const void* buffer, roUint64 bytesToWrite) = 0;
	virtual roUint64	posWrite		() const = 0;
	virtual Status		flush			() { return roStatus::ok; }
	virtual Status		closeWrite		() { return roStatus::ok; }

	template<roSize N>
			Status		write			(const roUtf8 (&ary)[N]) { return write(ary, N); }
};

struct MemoryIStream : public IStream
{
	MemoryIStream(roByte* buffer=NULL, roSize size=0);

			void		reset			(roByte* buffer=NULL, roSize size=0);
	virtual Status		size			(roUint64& bytes) const;
	virtual roUint64	posRead			() const { return current - begin; }
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin);
};

struct MemoryOStream : public OStream
{
	virtual Status		seekWrite		(roInt64 offset, SeekOrigin origin) { return roStatus::not_supported; }
	virtual Status		write			(const void* buffer, roUint64 bytesToWrite);
	virtual roUint64	posWrite		() const { return size(); }

	roByte*				bytePtr			() { return _buf.castedPtr<roByte>(); }
	roSize				size			() const { return _buf.sizeInByte(); }

	void				clear			() { _buf.clear(); }

// Private:
	ByteArray _buf;
};

struct MemorySeekableOStream : public OStream
{
	MemorySeekableOStream();
	
	virtual Status		seekWrite		(roInt64 offset, SeekOrigin origin);
	virtual Status		write			(const void* buffer, roUint64 bytesToWrite);
	virtual roUint64	posWrite		() const { return _pos; }
	
	roByte*				bytePtr			() { return _buf.castedPtr<roByte>(); }
	roSize				size			() const { return _buf.sizeInByte(); }

// Private:
	ByteArray _buf;
	roSize _pos;
};

Status openRawFileIStream	(roUtf8* path, AutoPtr<IStream>& stream, bool blocking=false);
Status openHttpIStream		(roUtf8* url, AutoPtr<IStream>& stream, bool blocking=false);

}   // namespace ro

#endif	// __roIOStream_h__
