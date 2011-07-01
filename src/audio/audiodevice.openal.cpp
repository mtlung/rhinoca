#include "pch.h"
#include "audiodevice.h"
#include "../array.h"
#include "../linklist.h"
#include "../resource.h"
#include "../vector.h"

#if defined(RHINOCA_APPLE)
#	include <OpenAL/al.h>
#	include <OpenAL/alc.h>
#else
#	include "../../thirdparty/OpenAL/al.h"
#	include "../../thirdparty/OpenAL/alc.h"
#endif

/*	Some useful links about OpenAL:
	Offset
	http://www.nabble.com/AL_*_OFFSET-resolution-td14216950.html
	Calculate the current playing time:
	http://opensource.creative.com/pipermail/openal/2008-March/011015.html
	Jump to particular position:
	http://stackoverflow.com/questions/434599/openal-how-does-one-jump-to-a-particular-offset-more-than-once
 */

#if defined(RHINOCA_VC)
#	pragma comment(lib, "OpenAL32")
#endif

using namespace Audio;

// Apple suggest to use it's OpenAL extension to avoid extra buffer memory copy
// http://developer.apple.com/library/ios/#technotes/tn2199/_index.html
#if defined(RHINOCA_APPLE)
typedef ALvoid AL_APIENTRY (*alBufferDataStaticProcPtr) (const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq);
ALvoid alBufferDataStatic(const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq)
{
	static alBufferDataStaticProcPtr proc = NULL;
    
    if(!proc)
        proc = (alBufferDataStaticProcPtr)alcGetProcAddress(NULL, "alBufferDataStatic");

    if(proc)
        proc(bid, format, data, size, freq);
	else
		alBufferData(bid, format, data, size, freq);
}

#define alBufferData alBufferDataStatic
#endif

static const char* getALErrorString(ALenum err)
{
	switch(err)
	{
		case AL_NO_ERROR:
			return "AL_NO_ERROR";
		case AL_INVALID_NAME:
			return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:
			return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:
			return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:
			return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:
			return "AL_OUT_OF_MEMORY";
	}
	return "";
}

static bool checkAndPrintError(const char* prefixMessage)
{
	ALenum err = alGetError();
	if(err == AL_NO_ERROR)
		return true;

	print(NULL, "%s%s\n", prefixMessage, getALErrorString(err));
	return false;
}

struct AlBuffer
{
	AlBuffer()
		: dataReady(false)
		, referenceCount(0)
		, begin(0), end(0)
		, handle(0)
	{}

	~AlBuffer()
	{
		ASSERT(referenceCount == 0);
	}

	bool dataReady;
	unsigned referenceCount;	/// How many AudioSound is using this buffer
	unsigned begin, end;
	ALuint handle;
	AudioBufferPtr srcData;
};	// AlBuffer

struct AudioSound : public LinkListBase::Node<AudioSound>
{
	explicit AudioSound(AudioDevice* d)
		: device(d)
		, isPlay(false)
		, isPause(true)	// NOTE: Paused by default
		, isLoop(false)
		, queueSampleStartPosition(0)
		, nextALBufLoadPosition(0)
	{
		alGenSources(1, &handle);
	}

	~AudioSound();

// Operations:
	/// If success, return the index to AudioDevice::_alBuffers
	int tryLoadNextBuffer();

	void unqueueBuffer();

// Attributes:
	struct Active : public LinkListBase::Node<AudioSound::Active>
	{
		DECLAR_GET_OUTER_OBJ(AudioSound, activeListNode);
		void destroyThis() {
			delete getOuterSafe();
		}
	} activeListNode;

	ALuint handle;
	AudioDevice* device;
	AudioBufferPtr audioBuffer;

	bool isPlay;
	bool isPause;
	bool isLoop;
	unsigned queueSampleStartPosition;
	unsigned nextALBufLoadPosition;

	unsigned totalSamples() const {
		return audioBuffer->totalSamples();
	}

	struct BufferIndex
	{
		BufferIndex() : index(-1), queued(false) {}
		int index;		/// Index to AudioDevice::_alBuffers, negative number represent null.
		bool queued;	/// Put to OpenAL buffer queue or not yet
	};

	static const unsigned MAX_AL_BUFFERS = 2;
	Array<BufferIndex, MAX_AL_BUFFERS> alBufferIndice;
};	// AudioSound

struct AudioDevice
{
	AudioDevice()
	{
	}

	~AudioDevice()
	{
		_soundList.destroyAll();

		for(unsigned i=0; i<MAX_AL_BUFFERS; ++i)
			alDeleteBuffers(1, &_alBuffers[i].handle);
		alcDestroyContext(alContext);
	}

// Operations:
	void makeContextCurrent()
	{
		alcMakeContextCurrent(alContext);
	}

	void initAlBuffers()
	{
		for(unsigned i=0; i<MAX_AL_BUFFERS; ++i)
			alGenBuffers(1, &_alBuffers[i].handle);
	}

	AudioSound* createSound(const char* uri, ResourceManager* resourceMgr);

	int allocateAlBufferFor(AudioBuffer* src, unsigned begin, unsigned end);
	void freeAlBuffer(int index);

	void update();

// Attributes:
	ALCcontext* alContext;

	LinkList<AudioSound> _soundList;
	LinkList<AudioSound::Active> _activeSoundList;

	static const unsigned MAX_AL_BUFFERS = 4;
	Array<AlBuffer, MAX_AL_BUFFERS> _alBuffers;
};	// AudioDevice

AudioSound::~AudioSound()
{
	// alDeleteSources() should handle stopping and unqueue buffer for us
	alDeleteSources(1, &handle);

	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		int idx = alBufferIndice[i].index;
		if(idx < 0) continue;
		device->_alBuffers[idx].referenceCount--;
	}
}

int AudioSound::tryLoadNextBuffer()
{
	// Look for empty buffer slot
	int slot = -1;
	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		if(alBufferIndice[i].index == -1) {
			slot = i;
			break;
		}
	}

	// Already enough buffer are prepared, so return
	if(slot < 0)
		return -1;

	// Search the next sample position to load for
	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		int j = alBufferIndice[i].index;
		if(j < 0) continue;

		AlBuffer& b = device->_alBuffers[j];
		if(b.end >= nextALBufLoadPosition)
			nextALBufLoadPosition = b.end;
	}

	AudioBuffer::Format format;
	audioBuffer->getFormat(format);

	// Audio format not ready
	if(format.channels == 0)
		return -1;

	// The existing buffers already cover the whole range
	if(nextALBufLoadPosition >= format.totalSamples && format.totalSamples > 0)
		return -1;

	// Try to load one second of sound data if the length of the audio is unknown
	unsigned sampleToLoad = format.totalSamples > 0 ? format.totalSamples : 1 * format.samplesPerSecond + nextALBufLoadPosition;

	// Query the device for any existing buffer for the requesting source and range,
	// return a new one if none had found.
	const int index = device->allocateAlBufferFor(audioBuffer.get(), nextALBufLoadPosition, sampleToLoad);

	if(index < 0)
		return -1;

	alBufferIndice[slot].index = index;

	return index;
}

void AudioSound::unqueueBuffer()
{
	ALuint bufferHandle = 0;
	alGetError();
	alSourceUnqueueBuffers(handle, 1, &bufferHandle);
	checkAndPrintError("alSourceUnqueueBuffers failed: ");

	for(unsigned i=0; i<AudioSound::MAX_AL_BUFFERS; ++i) {
		int idx = alBufferIndice[i].index;
		if(idx < 0) continue;
		AlBuffer& b = device->_alBuffers[idx];
		if(b.handle == bufferHandle) {
			queueSampleStartPosition = b.end;
			alBufferIndice[i].index = -1;
			alBufferIndice[i].queued = false;
			b.referenceCount--;
			break;
		}
	}
}

void audioSound_destroy(AudioSound* sound)
{
	if(sound)
		sound->destroyThis();
}

const char* audioSound_getUri(AudioSound* sound)
{
	if(!sound->audioBuffer) return "";
	return sound->audioBuffer->getKey().c_str();
}

AudioSound* AudioDevice::createSound(const char* uri, ResourceManager* resourceMgr)
{
	AudioSound* sound = new AudioSound(this);
	sound->audioBuffer = resourceMgr->loadAs<AudioBuffer>(uri);

	if(!sound->audioBuffer) {
		delete sound;
		sound = NULL;
	}
	else
		_soundList.pushBack(*sound);

	return sound;
}

int AudioDevice::allocateAlBufferFor(AudioBuffer* src, unsigned begin, unsigned end)
{
	ASSERT(src);

	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		AlBuffer& b = _alBuffers[i];

		// Reuse existing perfect matching duration buffer (mostly for non-streaming sound)
		if(b.begin == begin && b.end == end && b.srcData == src && b.dataReady) {
			b.referenceCount++;
			return i;
		}
	}

	// No free buffer, try to find unused one or free one with zero referenceCount
	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		AlBuffer& b = _alBuffers[i];
		if(!b.srcData || b.referenceCount == 0) {
			b.dataReady = false;
			b.srcData = src;
			b.begin = begin;
			b.end = end;
			b.referenceCount++;
			return i;
		}
	}

	// No more buffer to play around, the last audio request will be ignored
	print(NULL, "Not enough audio buffers for simultaneous sound play, last audio play request will be ignored\n");

	return -1;
}

void AudioDevice::freeAlBuffer(int index)
{
	if(index < 0) return;

	_alBuffers[index].referenceCount--;
	if(_alBuffers[index].referenceCount == 0)
		_alBuffers[index] = AlBuffer();
}

static ALenum getAlFormat(const AudioBuffer::Format& format)
{
	if(format.channels == 1) {
		if(format.bitsPerSample == 8)
			return AL_FORMAT_MONO8;
		else if(format.bitsPerSample == 16)
			return AL_FORMAT_MONO16;
	}
	else if(format.channels == 2) {
		if(format.bitsPerSample == 8)
			return AL_FORMAT_STEREO8;
		else if(format.bitsPerSample == 16)
			return AL_FORMAT_STEREO16;
	}

	return -1;
}

void AudioDevice::update()
{
	// Loop for the active sound list
	for(AudioSound::Active* n = _activeSoundList.begin(); n != _activeSoundList.end(); )
	{
		AudioSound::Active* next = n->next();

		// Remove any in-active sound
		AudioSound& sound = n->getOuter();
		if(!sound.isPlay) {
			sound.removeThis();
			n = next;
			continue;
		}

		ASSERT(sound.audioBuffer);
		sound.audioBuffer->hotness++;

		// Request the number of OpenAL Buffers have been processed (played) on the Source
		ALint buffersProcessed = 0;
		alGetSourcei(sound.handle, AL_BUFFERS_PROCESSED, &buffersProcessed);

		while(buffersProcessed--) {
			sound.unqueueBuffer();
			sound.tryLoadNextBuffer();
		}

		// If data is ready, queue the buffer to OpenAL
		for(unsigned i=0; i<AudioSound::MAX_AL_BUFFERS; ++i) {
			int idx = sound.alBufferIndice[i].index;
			if(idx < 0) continue;
			AlBuffer& b = _alBuffers[idx];
			if(b.dataReady && !sound.alBufferIndice[i].queued) {
				alSourceQueueBuffers(sound.handle, 1, &b.handle);

				int state;
				alGetSourcei(sound.handle, AL_SOURCE_STATE, &state);

				// We go inside this if the audio is:
				// 1) First time being play
				// 2) After a seek request (NOT implemented yet)
				if(state == AL_INITIAL) {
					sound.queueSampleStartPosition = b.begin;
					alSourcePlay(sound.handle);
					if(sound.isPause)
						alSourcePause(sound.handle);
				}

				checkAndPrintError("alSourceQueueBuffers failed: ");
				sound.alBufferIndice[i].queued = true;

				sound.nextALBufLoadPosition = b.end;
				sound.tryLoadNextBuffer();
			}
		}

		{	// State update
			ALint state;
			alGetSourcei(sound.handle, AL_SOURCE_STATE, &state);

			switch(state) {
			case AL_INITIAL:
				// NOTE: This state should rarely encountered in this switch-case block, 
				// since it's already handled near alSourceQueueBuffers()
				sound.tryLoadNextBuffer();
				break;
			case AL_PLAYING:
				if(sound.isPause)
					alSourcePause(sound.handle);
				break;
			case AL_PAUSED:
				if(sound.isPlay && !sound.isPause) {
					alSourcePlay(sound.handle);
					sound.activeListNode.removeThis();
				}
				break;
			case AL_STOPPED:
				// Detect the sound is stopped due to lack of buffer or really stopped.
				if(sound.isPlay && sound.totalSamples() == 0) {
					// Out of streaming data or other temporary interruption, keep retry
					alSourcePlay(sound.handle);
				}
				else if(sound.isPlay && audiodevice_getSoundCurrentSample(this, &sound) < sound.audioBuffer->totalSamples()) {
					// The sound is not to it's end yet, we are just hitting some temporary interruption
					alSourcePlay(sound.handle);
				}
				else if(sound.isLoop) {
					if(audiodevice_getSoundCurrentSample(this, &sound) >= sound.audioBuffer->totalSamples()) {
						alSourceRewind(sound.handle);
						sound.nextALBufLoadPosition = 0;
						sound.queueSampleStartPosition = 0;
					}
					alSourcePlay(sound.handle);
				}
				else
					sound.activeListNode.removeThis();

				if(sound.isPause)
					alSourcePause(sound.handle);

				break;
			default:
				break;
			}
		}

		n = next;
	}

	// Update the AL buffer list
	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i)
	{
		AlBuffer& b = _alBuffers[i];
		if(!b.srcData)
			continue;

		if(!b.dataReady) {
			unsigned readableSamples, readableBytes;
			AudioBuffer::Format format;

			if(void* readPtr = b.srcData->getReadPointerForRange(b.begin, b.end, readableSamples, readableBytes, format)) {
				alBufferData(b.handle, getAlFormat(format), readPtr, readableBytes, format.samplesPerSecond);
				b.dataReady = true;
				b.end = b.begin + readableSamples;
			}
		}
	}
}

static ALCdevice* _alcDevice = NULL;
static int _initCount = 0;

void audiodevice_init()
{
	if(_initCount > 0)
		return;

	ASSERT(!_alcDevice);
	_alcDevice = alcOpenDevice(NULL);

	if(!_alcDevice)
		print(NULL, "Fail to initialize audio device");
	else
		++_initCount;
}

void audiodevice_close()
{
	if(--_initCount)
		return;

	alcMakeContextCurrent(NULL);
	alcCloseDevice(_alcDevice);
	_alcDevice = NULL;
}

AudioDevice* audiodevice_create()
{
	AudioDevice* device = new AudioDevice;
	device->alContext = alcCreateContext(_alcDevice, NULL);
	device->makeContextCurrent();
	device->initAlBuffers();

	return device;
}

void audiodevice_destroy(AudioDevice* device)
{
	delete device;
}

AudioSound* audiodevice_createSound(AudioDevice* device, const char* uri, ResourceManager* resourceMgr)
{
	if(!device) return NULL;

	device->makeContextCurrent();
	return device->createSound(uri, resourceMgr);
}

void audiodevice_playSound(AudioDevice* device, AudioSound* sound)
{
	if(!sound->activeListNode.isInList())
		device->_activeSoundList.pushBack(sound->activeListNode);

	checkAndPrintError("alSourcePlay failed: ");
	sound->isPlay = true;
	sound->isPause = false;
}

void audiodevice_pauseSound(AudioDevice* device, AudioSound* sound)
{
	sound->isPause = true;
}

void audiodevice_stopSound(AudioDevice* device, AudioSound* sound)
{
	sound->isPlay = false;
}

void audiodevice_setSoundLoop(AudioDevice* device, AudioSound* sound, bool loop)
{
	sound->isLoop = loop;
	// We don't use OpenAl's loop because it doesn't handle streaming.
//	alSourcei(sound->handle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

bool audiodevice_getSoundLoop(AudioDevice* device, AudioSound* sound)
{
	return sound->isLoop;
}

void audiodevice_setSoundCurrentTime(AudioDevice* device, AudioSound* sound, float time)
{
}

float audiodevice_getSoundCurrentTime(AudioDevice* device, AudioSound* sound)
{
	AudioBuffer::Format format;
	sound->audioBuffer->getFormat(format);

	// Prevent divide by zero
	if(format.samplesPerSecond == 0)
		return 0;

	ALint offset;
	alGetSourcei(sound->handle, AL_SAMPLE_OFFSET, &offset);
	offset += sound->queueSampleStartPosition;

	int seconds = offset / format.samplesPerSecond;
	float fraction = float(offset % format.samplesPerSecond) / format.samplesPerSecond;

	return float(seconds) + fraction;
}

unsigned audiodevice_getSoundCurrentSample(AudioDevice* device, AudioSound* sound)
{
	ALint offset;
	alGetSourcei(sound->handle, AL_SAMPLE_OFFSET, &offset);
	return sound->queueSampleStartPosition + offset;
}

void audiodevice_setSoundvolume(AudioDevice* device, AudioSound* sound, float volume)
{
	alSourcef(sound->handle, AL_GAIN, volume);
}

float audiodevice_getSoundvolume(AudioDevice* device, AudioSound* sound)
{
	float ret = 0;
	alGetSourcef(sound->handle, AL_GAIN, &ret);
	return ret;
}

AudioBuffer* audiodevice_getSoundBuffer(AudioDevice* device, AudioSound* sound)
{
	return sound->audioBuffer.get();
}

void audiodevice_update(AudioDevice* device)
{
	device->makeContextCurrent();
	device->update();
}
