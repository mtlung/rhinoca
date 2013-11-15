#ifndef __roRingBuffer_h__
#define __roRingBuffer_h__

#include "roArray.h"

namespace ro {

///	A simple ring buffer
/// Note that this class is not thread safe,
/// you can't doing read and write on different threads
struct RingBuffer
{
	RingBuffer() { softLimit = 0; hardLimit = 0; clear(); }

	roStatus	write(roSize maxSizeToWrite, roByte*& writePtr);
	void		commitWrite(roSize written);

	/// Since we have multiple internal buffers, we need to keep calling read() until it returns NULL.
	/// If you need to get all the readable data in one go, call flushWrite() before read().
	roByte*		read(roSize& maxSizeToRead);
	void		commitRead(roSize read);

	/// Put all content in the write buffer to the read buffer.
	/// Useful when you want to get all readable data as a single contiguous block of memory.
	void		flushWrite();

	void		clear();

	roSize		memoryUsage() const;

	// When soft memory usage limit is reached, it should try to free some unused memory and
	// return BeaconStatus::retry_later for the Write() function
	roSize		softLimit;

	// When hard memory usage limit is reached, the write() function will return error.
	roSize		hardLimit;

// Private
	Array<roByte>& _rBuf() { return _buffers[_rBufIdx]; }
	Array<roByte>& _wBuf() { return _buffers[_wBufIdx]; }

	roSize _rBufIdx, _wBufIdx;
	roSize _rPos, _wPos;
	Array<roByte> _buffers[2];	/// Two buffers, one for read and one for write
};	// RingBuffer

inline roStatus RingBuffer::write(roSize maxSizeToWrite, roByte*& writePtr)
{
	roAssert(_wPos == _wBuf().size() && "Call commitWrite() for each write()");
	roStatus st = _wBuf().incSizeNoInit(maxSizeToWrite); if(!st) return st;
	writePtr = &_wBuf()[_wPos];
	return roStatus::ok;
}

inline void RingBuffer::commitWrite(roSize written)
{
	roAssert(_wPos + written <= _wBuf().size());
	_wPos += written;
	roVerify(_wBuf().resizeNoInit(_wPos));
}

inline roByte* RingBuffer::read(roSize& maxSizeToRead)
{
	roAssert(_rPos <= _rBuf().size());

	// No more to read from read buffer, swap with write buffer
	if(_rBuf().size() - _rPos == 0) {
		roSwap(_rBufIdx, _wBufIdx);
		_rPos = _wPos = 0;
		_wBuf().clear();
	}

	maxSizeToRead = _rBuf().size() - _rPos;
	if(!maxSizeToRead)
		return NULL;

	return &_rBuf()[_rPos];
}

inline void RingBuffer::commitRead(roSize read)
{
	roAssert(_rPos + read <= _rBuf().size());
	_rPos += read;

	roSize memUsage = memoryUsage();
	roAssert(memUsage >= _rPos);
	if(softLimit > 0 && memUsage >= softLimit && 
		(memUsage - _rPos) < softLimit	// See if we can have enough space to free and reach below soft limit
	) {
		roSize newSize = _rBuf().size() - _rPos;
		roMemmov(&_rBuf()[0], &_rBuf()[0] + _rPos, newSize);
		_rPos = 0;
		_rBuf().resize(newSize);
		_rBuf().condense();
	}
}

inline void RingBuffer::flushWrite()
{
	if(_rBufIdx == _wBufIdx)
		return;
	Array<roByte>& rb = _rBuf();
	Array<roByte>& wb = _wBuf();

	rb.insert(rb.size(), wb.begin(), wb.begin() + _wPos);
	_wPos = 0;
	wb.clear();
}

inline void RingBuffer::clear()
{
	_rBufIdx = 0;
	_wBufIdx = 1;
	_rPos = _wPos = 0;
	_rBuf().clear();
	_wBuf().clear();
}

inline roSize RingBuffer::memoryUsage() const
{
	return _buffers[0].capacity() + _buffers[1].capacity();
}

}	// namespace ro

#endif	// __roRingBuffer_h__
