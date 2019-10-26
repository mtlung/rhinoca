#ifndef __roRingBuffer_h__
#define __roRingBuffer_h__

#include "roArray.h"

namespace ro {

/// A simple FIFO byte buffer
/// For optimal memory usage, read all the buffer when ever possible.
/// Note that this class is not thread safe, you can't doing read and write on different threads
struct RingBuffer
{
	RingBuffer() { softLimit = 0; hardLimit = 0; clear(); }

	roStatus	write(roSize maxSizeToWrite, roByte*& outWritePtr);
	void		commitWrite(roSize written);

	/// Transfer all the data in this buffer to another buffer
	roStatus	writeTo(RingBuffer& buf);

	/// Since we have multiple internal buffers, we need to keep calling read() until it returns NULL.
	/// If you need to get all the readable data in one go, call flushWrite() before read().
	roByte*		read(roSize& outReadSize);

	/// Ensure the specified amount of data is available and contiguous
	roStatus	atomicRead(roSize& inoutReadSize, roByte*& outReadPtr);
	void		commitRead(roSize read);

	roSize		totalReadable() const;

	/// Put all content in the write buffer to the read buffer.
	/// Useful when you want to get all readable data as a single contiguous block of memory.
	roStatus	flushWrite();
	roStatus	flushWrite(roSize sizeToFlush);

	// Re-claim unused memory in the read buffer
	void		compactReadBuffer();

	void		clear();

	roSize		memoryUsage() const;

	// When soft memory usage limit is reached, it should try to free some unused memory and
	// return BeaconStatus::retry_later for the Write() function
	roSize		softLimit;

	// When hard memory usage limit is reached, the write() function will return error.
	roSize		hardLimit;

// Private
	ByteArray&	_rBuf() const { return _buffers[_rBufIdx]; }
	ByteArray&	_wBuf() const { return _buffers[_wBufIdx]; }

	roSize _rBufIdx, _wBufIdx;
	roSize _rPos, _wBeg, _wEnd;
	mutable ByteArray _buffers[2];	/// Two buffers, one for read and one for write
};	// RingBuffer

inline roStatus RingBuffer::write(roSize maxSizeToWrite, roByte*& writePtr)
{
	roAssert(_wEnd == _wBuf().size() && "Call commitWrite() for each write()");	// NOTE: We swap with read so read should also do the same

	if(maxSizeToWrite == 0)
		writePtr = NULL;
	else {
		roStatus st = _wBuf().incSizeNoInit(maxSizeToWrite); if(!st) return st;
		writePtr = &_wBuf()[_wEnd];
	}
	return roStatus::ok;
}

inline void RingBuffer::commitWrite(roSize written)
{
	roAssert(_wEnd + written <= _wBuf().size());
	_wEnd += written;
	roVerify(_wBuf().resizeNoInit(_wEnd));
}

inline roStatus RingBuffer::writeTo(RingBuffer& buf)
{
	roSize totalReadableSize = totalReadable();
	roSize writeRemain = totalReadableSize;
	roByte* wPtr = NULL;
	roStatus st = buf.write(totalReadableSize, wPtr);
	if(!st) return st;

	while(writeRemain) {
		roSize readSize = 0;
		roByte* rPtr = read(readSize);
		roAssert(rPtr);	// If writeRemain > 0, rPtr should not be null
		roMemcpy(wPtr, rPtr, readSize);
		roAssert(writeRemain >= readSize);
		writeRemain -= readSize;
		commitRead(readSize);
	}

	buf.commitWrite(totalReadableSize);
	return roStatus::ok;
}

inline roByte* RingBuffer::read(roSize& outReadSize)
{
	roAssert(_rPos <= _rBuf().size());
	roAssert(_wEnd == _wBuf().size() && "Call commitWrite() for each write()");

	// No more to read from read buffer, swap with write buffer
	if(_rBuf().size() - _rPos == 0) {
		_wBuf().resize(_wEnd);	// Just in-case write() and commitWrite() didn't match
		roSwap(_rBufIdx, _wBufIdx);
		_rPos = _wBeg;
		_wBeg = _wEnd = 0;
		_wBuf().clear();
	}

	outReadSize = _rBuf().size() - _rPos;
	if(!outReadSize)
		return NULL;

	return &_rBuf()[_rPos];
}

inline roStatus RingBuffer::atomicRead(roSize& inoutReadSize, roByte*& outReadPtr)
{
	roSize rBufSize = _rBuf().size() - _rPos;
	roSize wBufSize = _wEnd - _wBeg;
	if(inoutReadSize > (rBufSize + wBufSize))
		return roStatus::not_enough_data;

	if(rBufSize < inoutReadSize) {
		roStatus st = flushWrite(inoutReadSize - rBufSize);
		if(!st) return st;
	}

	outReadPtr = read(inoutReadSize);
	return roStatus::ok;
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

inline roSize RingBuffer::totalReadable() const
{
	roSize rBufSize = _rBuf().size() - _rPos;
	roSize wBufSize = _wEnd - _wBeg;
	return rBufSize + wBufSize;
}

inline roStatus RingBuffer::flushWrite()
{
	return flushWrite(_wEnd - _wBeg);
}

inline roStatus RingBuffer::flushWrite(roSize sizeToFlush)
{
	roAssert(_wEnd >= _wBeg);

	if(_rBufIdx == _wBufIdx)
		return roStatus::ok;

	ByteArray& rb = _rBuf();
	ByteArray& wb = _wBuf();

	sizeToFlush = roMinOf2(sizeToFlush, _wEnd - _wBeg);
	roStatus st = rb.insert(rb.size(), wb.begin() + _wBeg, sizeToFlush);
	if(!st) return st;

	_wBeg += sizeToFlush;

	if(_wBeg == _wEnd) {
		_wBeg = _wEnd = 0;
		wb.clear();
	}

	return roStatus::ok;
}

inline void RingBuffer::compactReadBuffer()
{
	ByteArray& rb = _rBuf();
	roSize size = rb.size() - _rPos;
	roMemmov(rb.begin(), rb.begin() + _rPos, size);
	rb.resize(size);
	_rPos = 0;
}

inline void RingBuffer::clear()
{
	_rBufIdx = 0;
	_wBufIdx = 1;
	_rPos = _wBeg = _wEnd = 0;
	_rBuf().clear();
	_wBuf().clear();
}

inline roSize RingBuffer::memoryUsage() const
{
	return _buffers[0].capacity() + _buffers[1].capacity();
}

}	// namespace ro

#endif	// __roRingBuffer_h__
