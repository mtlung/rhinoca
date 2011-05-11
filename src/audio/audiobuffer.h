#ifndef __AUDIOBUFFER_H__
#define __AUDIOBUFFER_H__

#include "../resource.h"
#include "../vector.h"

/// Basically this class store chunks of audio data,
/// 
class AudioBuffer : public Resource
{
public:
	explicit AudioBuffer(const char* uri);
	virtual ~AudioBuffer();

// Operations
	/// data should be allocated using rhinoca_malloc()
	void insertSubBuffer(unsigned begin, unsigned end, void* data);

	/// For an audio position, it gives you the index to the first fitted sub-buffer,
	/// returns -1 if no data available.
	/// If the returned sub-buffer didn't fullfill the requesting length, call
	/// this function again with the new 'begin' position.
	int requestDataForPosition(unsigned position);

	/// Remove sub-buffer that is not used for a period of time.
	void collectGarbage();

// Attributes
	struct SubBuffer
	{
		unsigned posBegin, posEnd;
		unsigned sizeInByte;
		float hotless;
		void* handle;
	};

	Vector<SubBuffer> subBuffers;
};	// AudioBuffer

typedef IntrusivePtr<AudioBuffer> AudioBufferPtr;

#endif	// __AUDIOBUFFER_H__
