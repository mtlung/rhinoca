#ifndef __BIPBUFFER_H__
#define __BIPBUFFER_H__

/*!	A buffer class best for writing and reading it alternatively
	See: http://www.codeproject.com/KB/IP/bipbuffer.aspx#xx4023729xx
	\code
	char _rawbuf[1024];
	BipBuffer buf;
	buf.init(_rawbuf, sizeof(_rawbuf), NULL);

	// Lets write something to the buffer
	const char src[] = "hello world!";
	std::string dest;

	while(true) {
		// Reserve space first
		unsigned reserved = 0;
		unsigned char* writePtr = buf.reserveWrite(sizeof(src), reserved);

		if(!writePtr)
			break;

		// Do the write operation
		const char* srcPtr = src;
		memcpy(writePtr, src, reserved);

		// In this case, we commit what we have reserved, but committing something
		// less than the reserved is allowed.
		buf.commitWrite(reserved);

		src += reserved;
	}

	// Lets read back the data in the buffer
	while(true) {
		unsigned readSize = 0;

		// Get a read pointer to the contiguous block of memory
		const unsigned char* readPtr = buf.getReadPtr(readSize);
		if(!readPtr)
			break;

		dest.append((const char*)readPtr, readSize);

		// Tell the buffer you finish the read
		buf.commitRead(readSize);
	}
	\endcode
 */
class BipBuffer
{
public:
	BipBuffer();

	~BipBuffer();

	typedef void* (*Realloc)(void*, unsigned);

	/// May fail if the new size is not large enough to fit committed write data.
	bool init(void* buf, unsigned size, Realloc reallocFunc = NULL);

// Operations:
	/// Fail if you haven't give it a realloc function at the constructor.
	bool reallocBuffer(unsigned buffersize);

	/// Reserves space in the buffer for a memory write operation
	/// Return NULL if no space can be allocated or a previous reservation has not been committed.
	unsigned char* reserveWrite(unsigned sizeToReserve, unsigned& actuallyReserved);

	/// Committing a size of 0 will release the reservation.
	/// Committing a size > than the reserved size will cause an assert in a debug build;
	/// in a release build, the actual reserved size will be used.
	void commitWrite(unsigned size);

	/// Keep calling this function (and commitRead) until NULL is returned.
	unsigned char* getReadPtr(unsigned& size);

	void commitRead(unsigned size);

// Attributes:
	/// Queries how much data (in total) can be read in the buffer.
	unsigned readableSize() const
	{
		return _za + _zb;
	}

	unsigned _getSpaceAfterA() const;
	unsigned _getBFreeSpace() const;

	unsigned char* _buffer;
	unsigned _bufLen;
	unsigned _ia, _za;	// Staring index of A and size of A
	unsigned _ib, _zb;	// In fact, _ib is always zero
	unsigned ixResrv;
	unsigned szResrv;
	Realloc _realloc;
};	// BipBuffer

#endif	// __BIPBUFFER_H__
