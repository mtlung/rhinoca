#include "pch.h"
#include "roAudioDriver.h"
#include "../base/roFileSystem.h"
#include "../base/roLinkList.h"
#include "../base/roLog.h"
#include "../base/roResource.h"
#include "../base/roTypeCast.h"
#include "../roSubSystems.h"

#if defined(__APPLE__)
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

#if roCOMPILER_VC
#	pragma comment(lib, "OpenAL32")
#endif

using namespace ro;

static DefaultAllocator _allocator;

static const char* _getALErrorString(ALenum err)
{
	switch(err) {
		case AL_NO_ERROR:			return "AL_NO_ERROR";
		case AL_INVALID_NAME:		return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:		return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:		return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:	return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:		return "AL_OUT_OF_MEMORY";
	}
	return "";
}

static bool _checkAndPrintError(const char* prefixMessage)
{
	ALenum err = alGetError();
	if(err == AL_NO_ERROR)
		return true;

	roLog("error", "%s%s\n", prefixMessage, _getALErrorString(err));
	return false;
}

struct AudioLoader;

/// Structure storing raw PCM data
struct AudioBuffer : public ro::Resource
{
	struct SubBuffer;

	explicit AudioBuffer(const char* uri);
	~AudioBuffer();

	void addSubBuffer(unsigned pcmPosition, const char* data, roSize sizeInByte);

	SubBuffer* findSubBuffer(unsigned pcmPosition);

	struct Format {
		unsigned channels;
		unsigned samplesPerSecond;
		unsigned bitsPerSample;
		unsigned blockAlignment;	/// >= channels * self->bitsPerSample / 8
		unsigned totalSamples;		/// Zero for unknown duration
		unsigned estimatedSamples;	/// A rough version of estimatedSamples, for showing to the user only, not used for play/stop logic
	};

	struct SubBuffer
	{
		unsigned posBegin, posEnd;
		float hotness;
		ALuint handle;
	};

	Format format;
	AudioLoader* loader;
	ro::Array<SubBuffer> subBuffers;
};

typedef ro::SharedPtr<AudioBuffer> AudioBufferPtr;

struct AudioLoader : public Task
{
	AudioLoader(AudioBuffer* b, ResourceManager* mgr)
		: stream(NULL), manager(mgr), audioBuffer(b)
	{
		roAssert(b && mgr);
		if(b) b->loader = this;
		roZeroMemory(&format, sizeof(format));
	}

	void requestPcm(unsigned pcmPos)
	{
		if(!pcmRequest.find(pcmPos)) {
			if(pcmRequest.isEmpty())
				manager->taskPool->resume(audioBuffer->taskLoaded);
			pcmRequest.pushBack(pcmPos);
		}
	}

	void* stream;
	ResourceManager* manager;
	AudioBufferPtr audioBuffer;
	AudioBuffer::Format format;
	Array<unsigned> pcmRequest;	/// Pcm offset that the driver want to load next
};

AudioBuffer::AudioBuffer(const char* uri)
	: Resource(uri)
	, loader(NULL)
{
	roZeroMemory(&format, sizeof(format));
}

AudioBuffer::~AudioBuffer()
{
	for(roSize i=0; i<subBuffers.size(); ++i)
		alDeleteBuffers(1, &subBuffers[i].handle);
	delete loader;
}

static ALenum _getAlFormat(const AudioBuffer::Format& format)
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

void AudioBuffer::addSubBuffer(unsigned pcmPosition, const char* data, roSize sizeInByte)
{
	unsigned pcmBegin = pcmPosition;
	unsigned pcmEnd = pcmPosition + sizeInByte / format.blockAlignment;

	roSize i, j;
	// Find the correct index to insert
	for(i=0, j=0; i<subBuffers.size(); ++i) {
		j = roMinOf2(i + 1, subBuffers.size()-1);

		if(subBuffers[i].posBegin <= pcmPosition && pcmPosition < subBuffers[j].posBegin)
			break;
	}

	// Trim begin
	if(subBuffers.isInRange(i) && pcmBegin < subBuffers[i].posEnd) {
		roSize sizeToTrim = format.blockAlignment * (subBuffers[i].posEnd - pcmBegin);
		data += sizeToTrim;
		sizeInByte -= sizeToTrim;
		pcmBegin = subBuffers[i].posEnd;
	}

	// Trim end
	if(i < j && subBuffers.isInRange(j) && pcmEnd > subBuffers[j].posBegin) {
		sizeInByte -= format.blockAlignment * (pcmEnd - subBuffers[j].posBegin);
		pcmEnd = subBuffers[j].posBegin;
	}

	SubBuffer subBuffer = { pcmBegin, pcmEnd, 1, 0 };
	alGenBuffers(1, &subBuffer.handle);
	alBufferData(subBuffer.handle, _getAlFormat(format), data, sizeInByte, format.samplesPerSecond);
	subBuffers.insert(roMinOf2(i + 1, subBuffers.size()), subBuffer);
}

AudioBuffer::SubBuffer* AudioBuffer::findSubBuffer(unsigned pcmPosition)
{
	for(roSize i=0; i<subBuffers.size(); ++i) {
		if(subBuffers[i].posBegin <= pcmPosition && pcmPosition < subBuffers[i].posEnd)
			return &subBuffers[i];
	}

	return NULL;
}

struct roADriverSoundSource {};
struct SoundSource : public roADriverSoundSource, ro::ListNode<SoundSource>
{
	SoundSource();
	~SoundSource();

	struct Active : public ro::ListNode<SoundSource::Active>
	{
		void destroyThis() {
			delete roMemberHost(SoundSource, activeListNode, this);
		}
	} activeListNode;

	ALuint handle;
	AudioBufferPtr audioBuffer;

	bool isPlay;
	bool isPause;
	bool isLoop;

	unsigned nextQueuePos;

	struct QueuedSubBuffer {
		ALuint handle;
		unsigned posBegin, posEnd;
	};
	Array<QueuedSubBuffer> queuedSubBuffers;
};

SoundSource::SoundSource()
	: handle(0)
	, isPlay(false)
	, isPause(true)	// NOTE: Paused by default
	, isLoop(false)
	, nextQueuePos(0)
{
	alGenSources(1, &handle);
}

SoundSource::~SoundSource()
{
	// alDeleteSources() should handle stopping and un-queue buffer for us
	alDeleteSources(1, &handle);
}

struct roAudioDriverImpl : public roAudioDriver
{
	void makeCurrent()
	{
		roAssert(alContext);
		alcMakeContextCurrent(alContext);
	}

	ALCcontext* alContext;
	LinkList<SoundSource> soundList;
	LinkList<SoundSource::Active> activeSoundList;
};

#include "roWaveLoader.openal.h"
#include "roMp3Loader.openal.h"

static void _registerAudioLoaders()
{
	if(!roSubSystems || !roSubSystems->resourceMgr) {
		roAssert(false);
		return;
	}

	static void* registeredResourceMgr = NULL;

	if(roSubSystems->resourceMgr == registeredResourceMgr)
		return;

	registeredResourceMgr = roSubSystems->resourceMgr;
	roSubSystems->resourceMgr->addExtMapping(extMappingWav);
	roSubSystems->resourceMgr->addExtMapping(extMappingMp3);
}

static roADriverSoundSource* _newSoundSource(roAudioDriver* self, const roUtf8* uri, const roUtf8* typeHint, bool streaming)
{
	roAudioDriverImpl* impl = static_cast<roAudioDriverImpl*>(self);
	if(!impl) return NULL;

	if(!roSubSystems) return NULL;
	if(!roSubSystems->resourceMgr) return NULL;

	impl->makeCurrent();
	_registerAudioLoaders();

	AutoPtr<SoundSource> ret = _allocator.newObj<SoundSource>();
	ret->audioBuffer = roSubSystems->resourceMgr->loadAs<AudioBuffer>(uri);

	impl->soundList.pushBack(*ret);

	return ret.unref();
}

static void _deleteSoundSource(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;

	_allocator.deleteObj(impl);
}

static void _soundSourceSeekPos(roADriverSoundSource* self, float time);

static void _playSoundSource(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;

	if(!impl->activeListNode.isInList()) {
		roAssert(impl->getList());
		LinkList<SoundSource>* list = impl->getList();
		roAudioDriverImpl* driver = roMemberHost(roAudioDriverImpl, soundList, list);
		driver->activeSoundList.pushBack(impl->activeListNode);
	}

	impl->isPlay = true;
	impl->isPause = false;
//	_soundSourceSeekPos(0);
}

static bool _soundSourceIsPlaying(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return false;
	return impl->isPlay;
}

static void _stopSoundSource(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;
	impl->isPlay = false;
	alSourceStop(impl->handle);
}

static void _soundSourceSetLoop(roADriverSoundSource* self, bool loop)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;
	impl->isLoop = loop;
}

void _soundSourceSetPause(roADriverSoundSource* self, bool pause)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;
	impl->isPause = pause;
}

static ALint _soundSourceTellPcmPos(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return 0;
	if(!impl->audioBuffer) return 0;

	ALint offset;
	alGetSourcei(impl->handle, AL_SAMPLE_OFFSET, &offset);
	offset += impl->queuedSubBuffers.isEmpty() ? impl->nextQueuePos : impl->queuedSubBuffers.front().posBegin;

	// If OpenAL just consumed all the queued buffers, it's position will be zero,
	// but we don't want to present it as end of the sample rather than zero
	if(offset == 0) {
		ALint state;
		alGetSourcei(impl->handle, AL_SOURCE_STATE, &state);
		if(state == AL_STOPPED)
			offset = impl->nextQueuePos;
	}

	return offset;
}

static float _soundSourceTellPos(roADriverSoundSource* self)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return 0;
	if(!impl->audioBuffer) return 0;

	const AudioBuffer::Format& format = impl->audioBuffer->format;

	// Prevent divide by zero
	if(format.samplesPerSecond == 0)
		return 0;

	ALint offset = _soundSourceTellPcmPos(self);

	int seconds = offset / format.samplesPerSecond;
	float fraction = float(offset % format.samplesPerSecond) / format.samplesPerSecond;

	return float(seconds) + fraction;
}

static void _soundSourceSeekPos(roADriverSoundSource* self, float time)
{
	SoundSource* impl = static_cast<SoundSource*>(self);
	if(!impl) return;
	if(!impl->audioBuffer) return;

	const AudioBuffer::Format& format = impl->audioBuffer->format;

	// Prevent divide by zero
	if(format.samplesPerSecond == 0)
		return;

	time = roMaxOf2(0.0f, time);
	const unsigned samplePos = unsigned(time * format.samplesPerSecond);

	// Ignore request that's beyond the audio length
	if(format.totalSamples > 0 && samplePos >= format.totalSamples)
		return;

	// Remove all queued buffers and reset the play position to beginning
	// Call alSourceStop() such that unqueueBuffer() can get effect
	alSourceStop(impl->handle);
	for(roSize i=0; i<impl->queuedSubBuffers.size(); ++i)
		alSourceUnqueueBuffers(impl->handle, 1, &impl->queuedSubBuffers[i].handle);
	impl->queuedSubBuffers.clear();

	// Call alSourceRewind() to make the sound go though the AL_INITIAL state
//	alSourceRewind(impl->handle);

	impl->nextQueuePos = samplePos;
}

static void _tick(roAudioDriver* self)
{
	roAudioDriverImpl* impl = static_cast<roAudioDriverImpl*>(self);
	if(!impl) return;

	// Loop for the active sound list
	for(SoundSource::Active* n = impl->activeSoundList.begin(); n != impl->activeSoundList.end(); )
	{
		SoundSource& sound = *(roMemberHost(SoundSource, activeListNode, n));
		SoundSource::Active* next = n->next();

		// Remove in-active sound from the active list
		if(!sound.isPlay) {
			sound.removeThis();
			n = next;
			continue;
		}

		if(!sound.audioBuffer) {
			roAssert(false);
			continue;
		}

		// Request the number of OpenAL Buffers have been processed (played) on the Source
		ALint buffersProcessed = 0;
		alGetSourcei(sound.handle, AL_BUFFERS_PROCESSED, &buffersProcessed);
		roAssert(buffersProcessed <= (ALint)sound.queuedSubBuffers.size());
		while(buffersProcessed) {
			alSourceUnqueueBuffers(sound.handle, 1, &sound.queuedSubBuffers.front().handle);
			sound.queuedSubBuffers.remove(0);
			--buffersProcessed;
		}

		// Just very few buffer remains, queue up more
		if(sound.queuedSubBuffers.size() < 3) {
			// Look for loaded buffers
			if(AudioBuffer::SubBuffer* subBuffer = sound.audioBuffer->findSubBuffer(sound.nextQueuePos)) {
				// TODO: Handle the case where subBuffer->posBegin didn't align with the requesting position
				if(subBuffer->posBegin != sound.nextQueuePos) {
				}

				alSourceQueueBuffers(sound.handle, 1, &subBuffer->handle);
				SoundSource::QueuedSubBuffer queueItem = { subBuffer->handle, subBuffer->posBegin, subBuffer->posEnd };
				sound.queuedSubBuffers.pushBack(queueItem);
				sound.nextQueuePos = subBuffer->posEnd;
			}
			else {
				AudioLoader* loader = sound.audioBuffer->loader;
				if(!loader)
					continue;

				unsigned totalSamples = sound.audioBuffer->format.totalSamples;
				if(totalSamples == 0 || sound.nextQueuePos < totalSamples)
					loader->requestPcm(sound.nextQueuePos);
			}
		}

		{	// State update
			ALint state;
			alGetSourcei(sound.handle, AL_SOURCE_STATE, &state);

			switch(state) {
			case AL_INITIAL:
				// When user try to call play() on an already playing sound, alSourceRewind() will trigger this state
				if(sound.isPlay)
					alSourcePlay(sound.handle);
				break;
			case AL_PLAYING:
				if(sound.isPause)
					alSourcePause(sound.handle);
				break;
			case AL_PAUSED:
				if(sound.isPlay && !sound.isPause)
					alSourcePlay(sound.handle);
				break;
			case AL_STOPPED:
				if(sound.isPlay)
					alSourcePlay(sound.handle);

				if( sound.audioBuffer->format.totalSamples != 0 &&
					_soundSourceTellPcmPos(&sound) >= (ALint)sound.audioBuffer->format.totalSamples)
				{
					// Now the audio reach it's end
//					alSourceRewind(sound.handle);
					sound.nextQueuePos = 0;
				}

				if(!sound.isLoop && sound.audioBuffer->format.totalSamples != 0) {
					sound.isPlay = false;
					sound.activeListNode.removeThis();
				}

				break;
			default:
				break;
			}
		}

		n = next;
	}
}

roAudioDriver* roNewAudioDriver()
{
	AutoPtr<roAudioDriverImpl> ret = _allocator.newObj<roAudioDriverImpl>();

	ret->alContext = NULL;

	// Setup the function pointers
	ret->newSoundSource = _newSoundSource;
	ret->deleteSoundSource = _deleteSoundSource;
	ret->playSoundSource = _playSoundSource;
	ret->soundSourceIsPlaying = _soundSourceIsPlaying;
	ret->stopSoundSource = _stopSoundSource;
	ret->soundSourceSetLoop = _soundSourceSetLoop;
	ret->soundSourceSetPause = _soundSourceSetPause;
	ret->soundSourceTellPos = _soundSourceTellPos;
	ret->soundSourceSeekPos = _soundSourceSeekPos;
	ret->tick = _tick;

	return ret.unref();
}

static ALCdevice* _alcDevice = NULL;
static int _initCount = 0;

void _alDeviceInit()
{
	if(_initCount > 0)
		return;

	roAssert(!_alcDevice);
	_alcDevice = alcOpenDevice(NULL);

	if(!_alcDevice)
		roLog("error", "Fail to initialize audio device\n");
	else
		++_initCount;
}

void _alDeviceClose()
{
	if(--_initCount)
		return;

	alcMakeContextCurrent(NULL);
	alcCloseDevice(_alcDevice);
	_alcDevice = NULL;
}

bool roInitAudioDriver(roAudioDriver* self, const char* options)
{
	roAudioDriverImpl* impl = static_cast<roAudioDriverImpl*>(self);
	if(!impl) return false;

	if(impl->alContext)
		return true;

	_alDeviceInit();
	impl->alContext = alcCreateContext(_alcDevice, NULL);
	_checkAndPrintError("roInitAudioDriver failed: ");
	roAssert(impl->alContext);

	return impl->alContext != NULL;
}

void roDeleteAudioDriver(roAudioDriver* self)
{
	roAudioDriverImpl* impl = static_cast<roAudioDriverImpl*>(self);
	if(!impl) return;

	impl->soundList.destroyAll();

	if(impl->alContext) {
		alcDestroyContext(impl->alContext);
		_alDeviceClose();
	}

	_allocator.deleteObj(impl);
}