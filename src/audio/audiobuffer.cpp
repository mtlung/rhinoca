#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"

AudioBuffer::AudioBuffer(const char* uri)
	: Resource(uri)
{
	Format f = { 0, 0, 0, 0, 0 };
	format = f;
}

AudioBuffer::~AudioBuffer()
{
	for(unsigned i=0; i<subBuffers.size(); ++i)
		rhinoca_free(subBuffers[i].data);

	if(scratch) {
		AudioLoader* loader = reinterpret_cast<AudioLoader*>(scratch);
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

void* AudioBuffer::getWritePointerForRange(unsigned begin, unsigned end)
{
	ScopeLock lock(mutex);

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.posBegin <= begin && b.posEnd >= end) {
			unsigned offset = format.blockAlignment * (begin - b.posBegin);
			return b.data + offset;
		}
	}

	unsigned size = format.blockAlignment * (end - begin);
	rhbyte* p = (rhbyte*)rhinoca_malloc(size);
	SubBuffer b = { begin, end, size, 999.0f, false, p };
	subBuffers.push_back(b);

	return p;
}

void AudioBuffer::commitWriteForRange(unsigned begin, unsigned end)
{
	ScopeLock lock(mutex);

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.posBegin == begin && b.posEnd == end) {
			b.readyForRead = true;
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
		if(b.posBegin <= begin && b.posEnd >= end) {
			// Data is reading but not yet finished
			if(!b.readyForRead)
				return NULL;

			readableSamples = b.posEnd - begin;
			readableBytes = readableSamples * format.blockAlignment;
			_format = format;

			return b.data + (begin - b.posBegin) * format.blockAlignment;
		}
	}

	// Inform the loader to do it's job
	if(scratch) {
		// TODO: Prevent loading (overlapped) data that already in other sub-buffers
		AudioLoader* loader = reinterpret_cast<AudioLoader*>(scratch);
		loader->loadDataForRange(begin, end);
	}

	return NULL;
}

void AudioBuffer::collectGarbage()
{
	ScopeLock lock(mutex);

	for(unsigned i=0; i<subBuffers.size(); ++i) {
		SubBuffer& b = subBuffers[i];
		if(b.hotless < 0.001f && b.data) {
			rhinoca_free(b.data);
			SubBuffer empty = { 0, 0, 0, 0, NULL };
			b = empty;
		}
	}
}
