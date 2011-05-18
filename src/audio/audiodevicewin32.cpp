#include "pch.h"
#include "audiodevice.h"
#include "../array.h"
#include "../linklist.h"
#include "../resource.h"
#include "../vector.h"

#if defined(RHINOCA_IOS)
#	include <OpenAL/al.h>
#	include <OpenAL/alc.h>
#else
#	include "../../thirdparty/OpenAL/al.h"
#	include "../../thirdparty/OpenAL/alc.h"
#endif

/*	Some usefull links about OpenAL:
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
		, currentSamplePosition(0)
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
	unsigned currentSamplePosition;

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

	int allocateAlBufferFor(AudioBuffer* src, unsigned begin, unsigned& end);
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
	unsigned nextLoadPosition = currentSamplePosition;
	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		int j = alBufferIndice[i].index;
		if(j < 0) continue;

		AlBuffer& b = device->_alBuffers[j];
		if(b.end > nextLoadPosition)
			nextLoadPosition = b.end + 1;
	}

	AudioBuffer::Format format;
	audioBuffer->getFormat(format);

	// Audio format not ready
	if(format.totalSamples == 0)
		return -1;

	// The existing buffers already cover the whole range
	if(nextLoadPosition >= format.totalSamples)
		return -1;

	// Query the device for any existing buffer for the requesting source and range,
	// return a new one if none had found.
	int index = device->allocateAlBufferFor(audioBuffer.get(), nextLoadPosition, format.totalSamples);

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

	_soundList.pushBack(*sound);

	return sound;
}

int AudioDevice::allocateAlBufferFor(AudioBuffer* src, unsigned begin, unsigned& end)
{
	ASSERT(src);

	for(unsigned i=0; i<MAX_AL_BUFFERS; ++i) {
		AlBuffer& b = _alBuffers[i];

		// Reuse existing
		if(b.srcData == src) {
			if(b.begin <= begin) {
				end = b.end;
				b.referenceCount++;
				return i;
			}
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

		// Request the number of OpenAL Buffers have been processed (played) on the Source
		ALint buffersProcessed = 0;
		alGetSourcei(sound.handle, AL_BUFFERS_PROCESSED, &buffersProcessed);

		while(buffersProcessed--) {
			sound.unqueueBuffer();
		}

		// Try to fill available buffer slots
		for(unsigned i=0; i<AudioSound::MAX_AL_BUFFERS; ++i) {
			if(sound.alBufferIndice[i].index < 0)
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
				if(state == AL_INITIAL) {
					alSourcePlay(sound.handle);
					if(sound.isPause)
						alSourcePause(sound.handle);
				}

				checkAndPrintError("alSourceQueueBuffers failed: ");
				sound.alBufferIndice[i].queued = true;
			}
		}

		{	// State update
			ALint state;
			alGetSourcei(sound.handle, AL_SOURCE_STATE, &state);

			switch(state) {
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
				if(sound.isPlay && sound.isLoop) {
					alSourceRewind(sound.handle);
					alSourcePlay(sound.handle);
					if(sound.isPause)
						alSourcePause(sound.handle);
				}
				else
					sound.activeListNode.removeThis();
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
	return 0;
}

void audiodevice_setSoundVolumn(AudioDevice* device, AudioSound* sound, float volumn)
{
}

float audiodevice_getSoundVolumn(AudioDevice* device, AudioSound* sound)
{
	return 1;
}

void audiodevice_update(AudioDevice* device)
{
	device->makeContextCurrent();
	device->update();
}
