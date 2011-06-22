#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "stb_vorbis.h"
#include <AudioToolbox/AudioFileStream.h>

using namespace Audio;

namespace Loader {

Resource* createNSAudio(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".aac") ||
	   uriExtensionMatch(uri, ".adts") ||
	   uriExtensionMatch(uri, ".aifc") ||
	   uriExtensionMatch(uri, ".aiff") ||
	   uriExtensionMatch(uri, ".caf") ||
	   uriExtensionMatch(uri, ".mp3") ||
	   uriExtensionMatch(uri, ".m4a") ||
	   uriExtensionMatch(uri, ".next") ||
	   uriExtensionMatch(uri, ".wav") ||
	   uriExtensionMatch(uri, ".wave"))
		return new Audio::AudioBuffer(uri);
	return NULL;
}

class NSAudioLoader : public AudioLoader
{
public:
	NSAudioLoader(Audio::AudioBuffer* buf, ResourceManager* mgr);

	~NSAudioLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
		if(afs) AudioFileStreamClose(afs);
	}

	void loadDataForRange(unsigned begin, unsigned end);

	void run(TaskPool* taskPool);
	void onDataReady(const rhbyte* data, unsigned len, unsigned numPacket, AudioStreamPacketDescription* inPacketDescriptions);

	void* stream;
	Audio::AudioBuffer* buffer;
	ResourceManager* manager;

	bool headerLoaded;
	Audio::AudioBuffer::Format format;

	unsigned currentSamplePos;

	AudioFileStreamID afs;
	int discontinueFlag;	// Discontinuation in the data supply to AudioFileStreamParseBytes()
};	// NSAudioLoader

#define PRINTERROR(LABEL) printf("%s err %4.4s %d\n", LABEL, (char*)&err, (int)err)
	
// This is called by audio file stream when it finds property values
static void propertyListenerProc(
	void* inClientData,
	AudioFileStreamID inAudioFileStream,
	AudioFileStreamPropertyID inPropertyID,
	UInt32* ioFlags)
{
	NSAudioLoader* self = reinterpret_cast<NSAudioLoader*>(inClientData);

	OSStatus err = noErr;

//	printf("found property '%4.4s'\n", (char*)&inPropertyID);
	
	switch(inPropertyID) {
	case kAudioFileStreamProperty_ReadyToProducePackets:
	{
		// http://cocoawithlove.com/2008/09/streaming-and-playing-live-mp3-stream.html
		self->discontinueFlag = kAudioFileStreamParseFlag_Discontinuity;

		AudioStreamBasicDescription asbd;
		UInt32 asbdSize = sizeof(asbd);
		err = AudioFileStreamGetProperty(inAudioFileStream, kAudioFileStreamProperty_DataFormat, &asbdSize, &asbd);
		if(err) { PRINTERROR("AudioFileStreamGetProperty"); goto Abort; }

		UInt64 totalAudioBytes = 0;
		AudioFileStreamGetProperty(inAudioFileStream, kAudioFileStreamProperty_AudioDataByteCount, &asbdSize, &totalAudioBytes);

		if(asbd.mBytesPerFrame == 0) {
			print(self->manager->rhinoca, "NSAudioLoader: VBR format is not yet supported: '%s'\n", self->buffer->uri().c_str());		
			goto Abort;
		}

		self->format.channels = asbd.mChannelsPerFrame;
		self->format.samplesPerSecond = asbd.mSampleRate;
		self->format.bitsPerSample = asbd.mBitsPerChannel;
		self->format.blockAlignment = asbd.mBytesPerFrame;
		self->format.totalSamples = totalAudioBytes / asbd.mBytesPerFrame;

		// NOTE: AudioBuffer::setFormat() is thread safe
		self->buffer->setFormat(self->format);
	}
	}
	return;

Abort:
	self->buffer->state = Resource::Aborted;
}

// This is called by audio file stream when it finds packets of audio
static void audioPacketsProc(
	void* inClientData,
	UInt32 inNumberBytes,
	UInt32 inNumberPackets,
	const void* inInputData,
	AudioStreamPacketDescription* inPacketDescriptions)
{
	NSAudioLoader* self = reinterpret_cast<NSAudioLoader*>(inClientData);
	self->onDataReady((rhbyte*)inInputData, inNumberBytes, inNumberPackets, inPacketDescriptions);
}

NSAudioLoader::NSAudioLoader(Audio::AudioBuffer* buf, ResourceManager* mgr)
	: stream(NULL)
	, buffer(buf)
	, manager(mgr)
	, headerLoaded(false)
	, currentSamplePos(0)
	, discontinueFlag(0)
{
	// create an audio file stream parser
	OSStatus err = AudioFileStreamOpen(
		this, propertyListenerProc, audioPacketsProc, 
		0, &afs
	);
	if(err) PRINTERROR("AudioFileStreamOpen");
}

void NSAudioLoader::loadDataForRange(unsigned begin, unsigned end)
{
	// Submit the range to the command queue
}

static const unsigned _dataChunkSize = 1024 * 64;

void NSAudioLoader::run(TaskPool* taskPool)
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = io_open(rh, buffer->uri(), tId);
	if(!stream) {
		print(rh, "NSAudioLoader: Fail to open file '%s'\n", buffer->uri().c_str());
		goto Abort;
	}

	if(!io_ready(stream, _dataChunkSize, tId))
		return reSchedule();

	{	// Read from stream and pass to audio parser
		rhbyte buf[_dataChunkSize];
		unsigned readCount = io_read(stream, buf, _dataChunkSize, tId);
		
		if(readCount == 0) {
			buffer->state = Resource::Loaded;
			return;
		}
		
		if(OSStatus err = AudioFileStreamParseBytes(afs, readCount, buf, discontinueFlag)) {
			PRINTERROR("AudioFileStreamParseBytes");
			goto Abort;
		}
		
		if(buffer->state == Resource::Aborted)
			goto Abort;
	}

	return reSchedule();

Abort:
	buffer->state = Resource::Aborted;
}

void NSAudioLoader::onDataReady(const rhbyte* data, unsigned len, unsigned numPacket, AudioStreamPacketDescription* inPacketDescriptions)
{
	if(inPacketDescriptions) for(int i=0; i<numPacket; ++i)
	{
//		SInt64 packetOffset = inPacketDescriptions[i].mStartOffset;
//		SInt64 packetSize = inPacketDescriptions[i].mDataByteSize;
		
//		memcpy((char*)fillBuf->mAudioData + myData->bytesFilled, (const char*)inInputData + packetOffset, packetSize);
//		packetSize = packetSize;
	}
	else
	{
		ASSERT(len % format.blockAlignment == 0);
		const unsigned sampleCount = len / format.blockAlignment;
		const unsigned audioBufBegin = currentSamplePos;
		const unsigned audioBufEnd = audioBufBegin + sampleCount;

		while(currentSamplePos < audioBufEnd) {
			unsigned bytesToWrite = 0;
			unsigned pos = audioBufEnd;

			void* bufferData = buffer->getWritePointerForRange(currentSamplePos, pos, bytesToWrite);
			ASSERT(bufferData);
			ASSERT(bytesToWrite <= len);

			memcpy(bufferData, data, bytesToWrite);
			buffer->commitWriteForRange(currentSamplePos, pos);

			data += bytesToWrite;
			len -= bytesToWrite;
			currentSamplePos = pos;
		}
	}

	discontinueFlag = 0;
}

bool loadNSAudio(Resource* resource, ResourceManager* mgr)
{
	const char* uri = resource->uri();
	if(uriExtensionMatch(uri, ".aac") ||
	   uriExtensionMatch(uri, ".adts") ||
	   uriExtensionMatch(uri, ".aifc") ||
	   uriExtensionMatch(uri, ".aiff") ||
	   uriExtensionMatch(uri, ".caf") ||
	   uriExtensionMatch(uri, ".mp3") ||
	   uriExtensionMatch(uri, ".m4a") ||
	   uriExtensionMatch(uri, ".next") ||
	   uriExtensionMatch(uri, ".wav") ||
	   uriExtensionMatch(uri, ".wave"))
	{
		TaskPool* taskPool = mgr->taskPool;
		
		Audio::AudioBuffer* buffer = dynamic_cast<Audio::AudioBuffer*>(resource);
		
		NSAudioLoader* loaderTask = new NSAudioLoader(buffer, mgr);
		buffer->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());					// Load meta data
		buffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, buffer->taskReady, ~taskPool->mainThreadId());	// All load completes
		
		return true;
	}

	return false;
}

}	// namespace Loader
