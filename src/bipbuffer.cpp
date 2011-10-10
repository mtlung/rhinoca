#include "pch.h"
#include "bipbuffer.h"

BipBuffer::BipBuffer()
	: _buffer(NULL), _bufLen(0), _realloc(NULL)
{
	clear();
}

BipBuffer::~BipBuffer()
{
	init(NULL, NULL);
}

bool BipBuffer::init(void* buf, unsigned size, Realloc reallocFunc)
{
	if(reallocFunc && !buf)
		buf = (unsigned char*)reallocFunc(buf, size);

	// Free the old buffer
	if(_realloc)
		_realloc(_buffer, 0);

	_buffer = (unsigned char*)buf;
	_bufLen = size;
	_realloc = reallocFunc;

	clear();

	return true;
}

bool BipBuffer::reallocBuffer(unsigned buffersize)
{
	if(buffersize <= 0 || !_realloc)
		return false;

	if(_buffer = (unsigned char*)_realloc(_buffer, buffersize)) {
		_bufLen = buffersize;
		return true;
	}

	return false;
}

unsigned char* BipBuffer::reserveWrite(unsigned sizeToReserve, unsigned& actuallyReserved)
{
	// We always allocate on B if B exists; this means we have two blocks and our buffer is filling.
	if(_zb)
	{
		unsigned freespace = _getBFreeSpace();

		if(sizeToReserve < freespace) freespace = sizeToReserve;

		if(freespace == 0) return NULL;

		szResrv = actuallyReserved = freespace;
		ixResrv = _ib + _zb;
		return _buffer + ixResrv;
	}
	// Block b does not exist, so we can check if the space AFTER a is bigger than the space
	// before A, and allocate the bigger one.
	else
	{
		unsigned freespace = _getSpaceAfterA();
		if(freespace >= _ia) {
			if(freespace == 0) return NULL;
			if(sizeToReserve < freespace) freespace = sizeToReserve;
//			if((sizeToReserve < freespace) || (freespace >= _ia)) freespace = sizeToReserve;

			szResrv = actuallyReserved = freespace;
			ixResrv = _ia + _za;
			return _buffer + ixResrv;
		}
		else {
			if(_ia == 0) return NULL;
			if(_ia < sizeToReserve) sizeToReserve = _ia;
			szResrv = actuallyReserved = sizeToReserve;
			ixResrv = 0;
			return _buffer;
		}
	}
}

unsigned char* BipBuffer::reserveWrite(unsigned& actuallyReserved)
{
	return reserveWrite(bufferSize(), actuallyReserved);
}

void BipBuffer::commitWrite(unsigned size)
{
	if(size == 0) {
		// undo any reservation
		szResrv = ixResrv = 0;
		return;
	}

	// If we try to commit more space than we asked for, clip to the size we asked for.
//	RHASSERT(size <= szResrv);
	if(size > szResrv)
		size = szResrv;

	// If we have no blocks being used currently, we create one in A.
	if(_za == 0 && _zb == 0) {
		_ia = ixResrv;
		_za = size;

		ixResrv = 0;
		szResrv = 0;
		return;
	}

	if(ixResrv == _za + _ia)
		_za += size;
	else
		_zb += size;

	ixResrv = 0;
	szResrv = 0;
}

unsigned char* BipBuffer::getReadPtr(unsigned& size)
{
	if(_za == 0) {
		size = 0;
		return NULL;
	}

	size = _za;
	return _buffer + _ia;
}

void BipBuffer::commitRead(unsigned size)
{
	if(size >= _za) {
		_ia = _ib;
		_za = _zb;
		_ib = 0;
		_zb = 0;
	}
	else {
		_za -= size;
		_ia += size;
	}
}

void BipBuffer::clear()
{
	_ia = _za = _ib = _zb = ixResrv = szResrv = 0;
}

unsigned BipBuffer::_getSpaceAfterA() const
{
	return _bufLen - _ia - _za;
}

unsigned BipBuffer::_getBFreeSpace() const
{
	return _ia - _ib - _zb;
}
