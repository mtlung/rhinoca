#ifndef __network_roHttpContentDecoder_h__
#define __network_roHttpContentDecoder_h__

#include "../base/roRingBuffer.h"

namespace ro {
namespace http {

struct IDecoder
{
	virtual roStatus	requestWrite		(roSize maxSizeToWrite, roByte*& outWritePtr) = 0;
	virtual void		commitWrite			(roSize written) = 0;
	virtual bool		keepWrite			() const = 0;

	virtual roStatus	requestRead			(roSize& outReadSize, roByte*& outReadPtr) = 0;
	virtual void		commitRead			(roSize read) = 0;
};

struct SizedDecoder : public IDecoder
{
	SizedDecoder();

	Status	requestWrite	(roSize maxSizeToWrite, roByte*& outWritePtr) override;
	void	commitWrite		(roSize written) override;
	bool	keepWrite		() const override;
	Status	requestRead		(roSize& outReadSize, roByte*& outReadPtr) override;
	void	commitRead		(roSize read) override;

	void	contentReadBegin();

	RingBuffer*	ringBuffer;
	bool		contentBegin;
	roUint64	expectedEnd;
	roUint64	bytesAlreadyWritten;
	roUint64	bytesAlreadyRead;
};	// SizedDecoder

struct ChunkedDecoder : public IDecoder
{
	ChunkedDecoder();

	Status	requestWrite	(roSize maxSizeToWrite, roByte*& outWritePtr) override;
	void	commitWrite		(roSize written) override;
	bool	keepWrite		() const override;
	Status	requestRead		(roSize& outReadSize, roByte*& outReadPtr) override;
	void	commitRead		(roSize read) override;

	RingBuffer*	ringBuffer;
	bool		ended;
	bool		chunkSizeKnown;
	roUint64	chunkSize;
};	// ChunkedDecoder

SizedDecoder::SizedDecoder()
	: ringBuffer(NULL)
	, contentBegin(false)
	, expectedEnd(0)
	, bytesAlreadyWritten(0)
	, bytesAlreadyRead(0)
{}

ChunkedDecoder::ChunkedDecoder()
	: ringBuffer(NULL)
	, ended(false)
	, chunkSizeKnown(false)
	, chunkSize(0)
{}

Status ChunkedDecoder::requestWrite(roSize maxSizeToWrite, roByte*& outWritePtr)
{
	return ringBuffer->write(maxSizeToWrite, outWritePtr);
}

void ChunkedDecoder::commitWrite(roSize written)
{
	ringBuffer->commitWrite(written);
}

bool ChunkedDecoder::keepWrite() const
{
	return !ended;
}

Status ChunkedDecoder::requestRead(roSize& outReadSize, roByte*& outReadPtr)
{
	if(!chunkSizeKnown) {
		roSize readSize = 0;
		roByte* rPtr = ringBuffer->read(readSize);
		if(!rPtr)
			return Status::in_progress;
	}

	return Status::ok;
}

void ChunkedDecoder::commitRead(roSize read)
{
	ringBuffer->commitRead(read);
}


Status SizedDecoder::requestWrite(roSize maxSizeToWrite, roByte*& outWritePtr)
{
	return ringBuffer->write(maxSizeToWrite, outWritePtr);
}

void SizedDecoder::commitWrite(roSize written)
{
	ringBuffer->commitWrite(written);
	bytesAlreadyWritten += written;
}

bool SizedDecoder::keepWrite() const
{
	return !contentBegin || bytesAlreadyWritten < expectedEnd;
}

Status SizedDecoder::requestRead(roSize& outReadSize, roByte*& outReadPtr)
{
	roAssert(bytesAlreadyRead <= expectedEnd);

	if(contentBegin && bytesAlreadyRead >= expectedEnd)
		return Status::end_of_data;

	outReadPtr = ringBuffer->read(outReadSize);
	return Status::ok;
}

void SizedDecoder::commitRead(roSize read)
{
	ringBuffer->commitRead(read);
	bytesAlreadyRead += read;
}

void SizedDecoder::contentReadBegin()
{
	contentBegin = true;
	expectedEnd += bytesAlreadyRead;
}

}   // namespace http
}   // namespace ro

#endif	// __network_roHttpContentDecoder_h__
