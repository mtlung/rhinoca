#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"

namespace Audio {

AudioBuffer::AudioBuffer(const char* uri)
	: Resource(uri)
{
	Format f = { 0, 0, 0, 0, 0, 0 };
	format = f;
}

AudioBuffer::~AudioBuffer()
{
	for(unsigned i=0; i<subBuffers.size(); ++i)
		rhinoca_free(subBuffers[i].data);

	if(scratch) {
		Loader::AudioLoader* loader = reinterpret_cast<Loader::AudioLoader*>(scratch);
		delete loader;
	} 
}

void AudioBuffer::setFormat(const Format& _format)
{
	ScopeLock lock(mutex);
	format = _format;
}

void AudioBuffer::getFormat(Format& _format)
{
	ScopeLock lock(mutex);
	_format = format;
}

unsigned AudioBuffer::sizeInByteForSamples(unsigned samples) const
{
	ScopeLock lock(mutex);
	return format.blockAlignment * samples;
}

void* AudioBuffer::getWritePointerForRange(unsigned begin, unsigned& end, unsigned& bytesToWrite)
{
	ScopeLock lock(mutex);

	if(begin == end || format.blockAlignment == 0) {
		bytesToWrite = 0;
		return NULL;
	}

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.posBegin <= begin && begin < b.posEnd) {
			end = b.posEnd;
			bytesToWrite = format.blockAlignment * (end - begin);
			const unsigned offset = format.blockAlignment * (begin - b.posBegin);
			return b.data + offset;
		}
	}

	bytesToWrite = format.blockAlignment * (end - begin);
	rhbyte* p = (rhbyte*)rhinoca_malloc(bytesToWrite);
	SubBuffer b = { begin, end, bytesToWrite, 999.0f, false, p };

	ASSERT(p);
	subBuffers.push_back(b);

	return p;
}

void AudioBuffer::commitWriteForRange(unsigned begin, unsigned end)
{
	ScopeLock lock(mutex);

	ASSERT(begin < end);

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(!b.readyForRead && b.posBegin == begin && end <= b.posEnd) {
			// Call realloc to reclaim wasted space
			b.data = (rhbyte*)rhinoca_realloc(b.data, format.blockAlignment * (b.posEnd - b.posBegin), format.blockAlignment * (end - begin));
			b.readyForRead = true;
			b.posEnd = end;
			return;
		}
	}

	ASSERT(false);
}

void* AudioBuffer::getReadPointerForRange(unsigned begin, unsigned end, unsigned& readableSamples, unsigned& readableBytes, Format& _format)
{
	ScopeLock lock(mutex);

	// Look for existing loaded buffer
	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.posBegin <= begin && begin < b.posEnd) {
			// Data is reading but not yet finished
			if(!b.readyForRead)
				return NULL;

			readableSamples = b.posEnd - begin;
			readableBytes = readableSamples * format.blockAlignment;
			_format = format;
			b.hotness++;

			return b.data + (begin - b.posBegin) * format.blockAlignment;
		}
	}

	// Inform the loader to do it's job
	if(scratch) {
		// TODO: Prevent loading (overlapped) data that already in other sub-buffers
		Loader::AudioLoader* loader = reinterpret_cast<Loader::AudioLoader*>(scratch);
		loader->loadDataForRange(begin, end);
	}

	return NULL;
}

unsigned AudioBuffer::memoryUsed() const
{
	ScopeLock lock(mutex);

	unsigned size = 0;
	for(unsigned i=0; i<subBuffers.size(); ++i) {
		const SubBuffer& b = subBuffers[i];
		size += b.sizeInByte;
	}
	return size;
}

void AudioBuffer::collectGarbage()
{
	ScopeLock lock(mutex);

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.hotness < 0.001f && b.data) {
			rhinoca_free(b.data);
			SubBuffer empty = { 0, 0, 0, 0, NULL };
			b = empty;
		}
	}
}

unsigned AudioBuffer::totalSamples() const
{
	ScopeLock lock(mutex);
	return format.totalSamples;
}

}	// namespace Audio
