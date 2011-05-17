#ifndef __AUDIO_AUDIOBUFFER_H__
#define __AUDIO_AUDIOBUFFER_H__

#include "../resource.h"
#include "../vector.h"

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
	};

// Operations
	void setFormat(const Format& format);

	void getFormat(Format& format);

	unsigned sizeInByteForSamples(unsigned samples) const;

	/// Returns a block of memory for the specific sample range.
	void* getWritePointerForRange(unsigned begin, unsigned end);

	/// For loader to tell the buffer data is written to the memory allocated via getWritePointerForRange()
	void commitWriteForRange(unsigned begin, unsigned end);

	/// Is data ready for read, null if data not ready.
	void* getReadPointerForRange(unsigned begin, unsigned end, unsigned& readableSamples, unsigned& readableBytes, Format& format);

	/// Remove sub-buffer that is not used for a period of time.
	void collectGarbage();

// Attributes
	void* loader;	/// Implementation specific loader

protected:
	Format format;

	struct SubBuffer
	{
		unsigned posBegin, posEnd;
		unsigned sizeInByte;
		float hotless;
		bool readyForRead;
		rhbyte* data;
	};

	Vector<SubBuffer> subBuffers;

	mutable Mutex mutex;
};	// AudioBuffer

typedef IntrusivePtr<AudioBuffer> AudioBufferPtr;

#endif	// __AUDIO_AUDIOBUFFER_H__
