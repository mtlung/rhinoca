#ifndef __AUDIOBUFFER_H__
#define __AUDIOBUFFER_H__

#include "common.h"
#include "resource.h"

class AudioBuffer : public Resource
{
public:
	explicit AudioBuffer(const char* uri);
	virtual ~AudioBuffer();

// Operations

// Attributes
	void* handle;
};	// AudioBuffer

typedef IntrusivePtr<AudioBuffer> AudioBufferPtr;

#endif	// __AUDIOBUFFER_H__
