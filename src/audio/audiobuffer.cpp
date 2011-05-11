#include "pch.h"
#include "audiobuffer.h"

AudioBuffer::AudioBuffer(const char* uri)
	: Resource(uri)
{
}

AudioBuffer::~AudioBuffer()
{
	for(unsigned i=0; i<subBuffers.size(); ++i)
		rhinoca_free(subBuffers[i].handle);
}

void AudioBuffer::insertSubBuffer(unsigned begin, unsigned end, void* data)
{
	SubBuffer b = { begin, end, 0, 999.0f, data };

	// Search for a free slot
	for(unsigned i=0; i<subBuffers.size(); ++i) {
		if(!subBuffers[i].handle) {
			subBuffers[i] = b;
			return;
		}
	}

	subBuffers.push_back(b);
}

int AudioBuffer::requestDataForPosition(unsigned pos)
{
	for(unsigned i=0; i<subBuffers.size(); ++i) {
		if(subBuffers[i].posBegin <= pos && subBuffers[i].posEnd >= pos)
			return int(i);
	}
	return -1;
}

void AudioBuffer::collectGarbage()
{
	for(unsigned i=0; i<subBuffers.size(); ++i) {
		if(subBuffers[i].hotless < 0.001f && subBuffers[i].handle) {
			rhinoca_free(subBuffers[i].handle);
			SubBuffer b = { 0, 0, 0, 0, NULL };
			subBuffers[i] = b;
		}
	}
}
