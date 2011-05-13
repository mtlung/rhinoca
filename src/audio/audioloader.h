#ifndef __AUDIO_AUDIOLOADER_H__
#define __AUDIO_AUDIOLOADER_H__

#include "../taskpool.h"

/// 
class AudioLoader : public Task
{
public:
	virtual ~AudioLoader() {}

// Operations
	virtual void loadDataForRange(unsigned begin, unsigned end) = 0;

// Attributes
};	// AudioLoader

#endif	// __AUDIO_AUDIOLOADER_H__
