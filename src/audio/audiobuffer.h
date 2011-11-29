#ifndef __AUDIO_AUDIOBUFFER_H__
#define __AUDIO_AUDIOBUFFER_H__

#include "../resource.h"
#include "../../roar/base/roArray.h"

namespace Audio {

/// Basically this class store chunks of audio data,
/// All the functions can be invoked in multi-thread environment.
class AudioBuffer : public Resource
{
public:
	explicit AudioBuffer(const char* uri);
	virtual ~AudioBuffer();

	struct Format {
		unsigned channels;
		unsigned samplesPerSecond;
		unsigned bitsPerSample;
		unsigned blockAlignment;	/// >= channels * self->bitsPerSample / 8
		unsigned totalSamples;		/// Zero for unknown duration
		unsigned estimatedSamples;	/// A rough version of estimatedSamples, for showing to the user only, not used for play/stop logic
	};

// Operations
	void setFormat(const Format& format);

	void getFormat(Format& format);

	unsigned sizeInByteForSamples(unsigned samples) const;

	/// Returns a block of memory for the specific sample range.
	/// Sometimes the buffer returned is less then the request, so you need to consult bytesToWrite.
	/// You can cancel the requested buffer by calling commitWriteForRange(begin, begin);
	void* getWritePointerForRange(unsigned begin, unsigned& end, unsigned& bytesToWrite);

	/// For loader to tell the buffer data is written to the memory allocated via getWritePointerForRange()
	/// To avoid memory waste, the better to be the same as specified in getWritePointerForRange()
	void commitWriteForRange(unsigned begin, unsigned end);

	/// Is data ready for read, null if data not ready.
	void* getReadPointerForRange(unsigned begin, unsigned end, unsigned& readableSamples, unsigned& readableBytes, Format& format);

	/// Calculate how much memory was spend in this audio buffer
	unsigned memoryUsed() const;

	/// Remove sub-buffer that is not used for a period of time.
	void collectGarbage();

// Attributes
	unsigned totalSamples() const;

protected:
	Format format;

	struct SubBuffer
	{
		unsigned posBegin, posEnd;
		unsigned sizeInByte;
		float hotness;
		bool readyForRead;
		rhbyte* data;
	};

	ro::Array<SubBuffer> subBuffers;

	mutable ro::Mutex mutex;
};	// AudioBuffer

typedef IntrusivePtr<AudioBuffer> AudioBufferPtr;

}	// namespace

#endif	// __AUDIO_AUDIOBUFFER_H__
