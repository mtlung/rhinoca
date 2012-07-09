#ifndef __roRingBuffer_h__
#define __roRingBuffer_h__

#include "roArray.h"

namespace ro {

///	A simple ring buffer
struct RingBuffer
{
	RingBuffer() { clear(); }

	roByte* write(roSize maxSizeToWrite)
	{
		roAssert(_wPos == _wBuf().size() && "Call commitWrite() for each write()");
		_wBuf().incSize(maxSizeToWrite);
		return &_wBuf()[_wPos];
	}

	void commitWrite(roSize written)
	{
		roAssert(_wPos + written <= _wBuf().size());
		_wPos += written;
		_wBuf().resize(_wPos);
	}

	/// Since we have multiple internal buffers, we need to keep calling read() until it returns NULL.
	/// If you need to get all the readable data in one go, call flushWrite() before read().
	roByte* read(roSize& maxSizeToRead)
	{
		roAssert(_rPos <= _rBuf().size());

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

	void commitRead(roSize read)
	{
		roAssert(_rPos + read <= _rBuf().size());
		_rPos += read;
	}

	/// Put all content in the write buffer to the read buffer.
	/// Useful when you want to get all readable data as a single contiguous block of memory.
	void flushWrite()
	{
		if(_rBufIdx == _wBufIdx)
			return;
		Array<roByte>& rb = _rBuf();
		Array<roByte>& wb = _wBuf();

		rb.insert(rb.size(), wb.begin(), wb.begin() + _wPos);
		_wPos = 0;
		wb.clear();
	}

	void clear()
	{
		 _rBufIdx = 0;
		 _wBufIdx = 1;
		 _rPos = _wPos = 0;
		 _rBuf().clear();
		 _wBuf().clear();
	}

// Private
	Array<roByte>& _rBuf() { return _buffers[_rBufIdx]; }
	Array<roByte>& _wBuf() { return _buffers[_wBufIdx]; }

	roSize _rBufIdx, _wBufIdx;
	roSize _rPos, _wPos;
	Array<roByte> _buffers[2];	/// Two buffers, one for read and one for write
};	// RingBuffer

}	// namespace ro

#endif	// __roRingBuffer_h__
