#ifndef __roIOStream_h__
#define __roIOStream_h__

#include "roArray.h"
#include "roNonCopyable.h"

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
	virtual ~IStream() {}
	virtual bool		readWillBlock	(roUint64 bytesToRead) { return false; }
	virtual Status		size			(roUint64& bytes) { return roStatus::not_supported; }
	virtual roUint64	posRead			() const = 0;
	virtual Status		seekRead		(roInt64 offset, SeekOrigin origin) { return roStatus::not_supported; }
	virtual void		closeRead		() {}

			Status		read			(void* buffer, roUint64 bytesToRead, roUint64& bytesRead);
			Status		atomicRead		(void* buffer, roUint64 bytesToRead);

// Typed read function
	template<class T>
			Status		read			(T& val) { return atomicRead(&val, sizeof(T)); }

// Attributes
	Status	st;
	roByte*	_begin;
	roByte*	_current;
	roByte*	_end;
	Status (*_next)(IStream& s);
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
	virtual void		flush			() {}
	virtual void		closeWrite		() {}

	template<roSize N>
			Status		write			(const roUtf8 (&ary)[N]) { return write(ary, N); }
};

struct MemoryIStream : public IStream
{
	MemoryIStream(roByte* buffer=NULL, roSize size=0);

			void		reset			(roByte* buffer=NULL, roSize size=0);
	virtual Status		size			(roUint64& bytes) const;
	virtual roUint64	posRead			() const { return _current - _begin; }
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

}   // namespace ro

#endif	// __roIOStream_h__
