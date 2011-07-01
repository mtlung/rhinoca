#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "stb_vorbis.h"
#include <string.h>	// for memcpy()

using namespace Audio;

namespace Loader {

struct RingBuffer
{
	RingBuffer()
		: buffer(NULL)
		, readPos(0), writePos(0), endPos(0)
	{}

	~RingBuffer()
	{
		rhinoca_free(buffer);
	}

	rhbyte* write(unsigned maxSizeToWrite)
	{
		if(writePos + maxSizeToWrite >= endPos) {
			buffer = (rhbyte*)rhinoca_realloc(buffer, endPos, endPos + maxSizeToWrite);
			endPos += maxSizeToWrite;
		}
		return buffer + writePos;
	}

	void commitWrite(unsigned written)
	{
		ASSERT(writePos + written <= endPos);
		writePos += written;
	}

	rhbyte* read(unsigned& maxSizeToRead)
	{
		maxSizeToRead = writePos - readPos;
		return buffer + readPos;
	}

	void commitRead(unsigned read)
	{
		ASSERT(readPos + read <= writePos);
		readPos += read;
	}

	void collectUnusedSpace()
	{
		// Move remaining data at the back to the font space
		unsigned sizeToMove = writePos - readPos;
		memcpy(buffer, buffer + readPos, sizeToMove);
		readPos = 0;
		writePos = sizeToMove;
	}

	rhbyte* buffer;
	unsigned readPos, writePos, endPos;
};	// RingBuffer

Resource* createOgg(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".ogg"))
		return new AudioBuffer(uri);
	return NULL;
}

class OggLoader : public AudioLoader
{
public:
	OggLoader(AudioBuffer* buf, ResourceManager* mgr)
		: stream(NULL)
		, buffer(buf)
		, manager(mgr)
		, headerLoaded(false)
		, currentSamplePos(0)
		, vorbis(NULL)
	{
	}

	~OggLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
		if(vorbis) stb_vorbis_close(vorbis);
	}

	void loadDataForRange(unsigned begin, unsigned end);

	void run(TaskPool* taskPool);

	void loadHeader();
	void loadData();

	void* stream;
	AudioBuffer* buffer;
	ResourceManager* manager;

	bool headerLoaded;
	AudioBuffer::Format format;

	unsigned currentSamplePos;

	stb_vorbis* vorbis;
	stb_vorbis_info vorbisInfo;

	RingBuffer ringBuffer;
};

void OggLoader::loadDataForRange(unsigned begin, unsigned end)
{
	// Submit the range to the command queue
}

void OggLoader::run(TaskPool* taskPool)
{
	if(!buffer->scratch)
		loadHeader();
	else
		loadData();
}

// This will be the approximate duration (in seconds) of a audio buffer segment to send to audio device
static const float _bufferDuration = 1.0f;

static const unsigned _dataChunkSize = 1024 * 16;

void OggLoader::loadHeader()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = io_open(rh, buffer->uri(), tId);
	if(!stream) {
		print(rh, "OggLoader: Fail to open file '%s'\n", buffer->uri().c_str());
		goto Abort;
	}

	if(!io_ready(stream, _dataChunkSize, tId))
		return reSchedule();

	{	// Read from stream and put to ring buffer
		rhbyte* p = ringBuffer.write(_dataChunkSize);
		unsigned readCount = io_read(stream, p, _dataChunkSize, tId);
		ringBuffer.commitWrite(readCount);

		// Read from ring buffer and put to vorbis
		p = ringBuffer.read(readCount);
		int error, byteUsed;
		vorbis = stb_vorbis_open_pushdata(p, readCount, &byteUsed, &error, NULL);

		if(error == VORBIS_need_more_data)
			return reSchedule();
		else if(!vorbis || error != 0)
			goto Abort;

		// Reading of header success
		ringBuffer.commitRead(byteUsed);
		ringBuffer.collectUnusedSpace();
		vorbisInfo = stb_vorbis_get_info(vorbis);

		format.channels = vorbisInfo.channels;
		format.samplesPerSecond = vorbisInfo.sample_rate;
		format.bitsPerSample = 16;
		format.blockAlignment = vorbisInfo.channels * 2;
		format.totalSamples = 0;

		// NOTE: AudioBuffer::setFormat() is thread safe
		buffer->setFormat(format);

		buffer->scratch = this;
	}

	return;

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

void OggLoader::loadData()
{
	int tId = TaskPool::threadId();
	void* bufferData = NULL;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) goto Abort;

	if(!io_ready(stream, _dataChunkSize, tId))
		return reSchedule();

	{	// Read from stream and put to ring buffer
		rhbyte* p = ringBuffer.write(_dataChunkSize);
		unsigned readCount = io_read(stream, p, _dataChunkSize, tId);
		ringBuffer.commitWrite(readCount);

		if(readCount == 0) {	// EOF
			format.totalSamples = currentSamplePos;
			buffer->setFormat(format);
			buffer->state = Resource::Loaded;
			return;
		}

		const unsigned proximateBufDuration = _bufferDuration * format.samplesPerSecond;
		unsigned bytesToWrite = 0;
		const int numChannels = vorbisInfo.channels <= 2 ? vorbisInfo.channels : 2; 
		p = ringBuffer.read(readCount);

		// Loop until we used up the available buffer we already read
		for(bool needMoreData = false; !needMoreData; )
		{
			unsigned audioBufBegin = currentSamplePos;
			unsigned audioBufEnd = audioBufBegin + proximateBufDuration;

			// Reserve a large enough buffer for 1 second audio
			bufferData = buffer->getWritePointerForRange(audioBufBegin, audioBufEnd, bytesToWrite);

			// Somethings goes wrong and making the audio buffer fail to allocate a single large buffer
			if(!bufferData)
				goto Abort;

			// Read from ring buffer and put to vorbis
			float** outputs = NULL;
			int sampleCount = 0;
			int byteUsed = 0;

			// Loop until we fill up the allocated audio buffer
			while(true)
			{
				byteUsed = stb_vorbis_decode_frame_pushdata(vorbis, p, readCount, NULL, &outputs, &sampleCount);

				// Summit the buffer decoded so far if we come across at AudioBuffer boundary
				if(currentSamplePos + sampleCount > audioBufEnd) {
					buffer->commitWriteForRange(audioBufBegin, currentSamplePos);
					audioBufBegin = currentSamplePos;
					audioBufEnd = currentSamplePos + proximateBufDuration;
					bufferData = buffer->getWritePointerForRange(currentSamplePos, audioBufEnd, bytesToWrite);
					if(!bufferData)
						goto Abort;
				}

				stb_vorbis_channels_short_interleaved(numChannels, (short*)bufferData, vorbisInfo.channels, outputs, 0, sampleCount);

				if(byteUsed == 0) {
					const int error = stb_vorbis_get_error(vorbis);
					if(error == VORBIS__no_error || error == VORBIS_need_more_data) {
						needMoreData = true;
						break;
					}
					else
						goto Abort;
				}

				p += byteUsed;
				readCount -= byteUsed;
				currentSamplePos += sampleCount;
				ringBuffer.commitRead(byteUsed);
				bufferData = (short*)(bufferData) + sampleCount * numChannels;

				// NOTE: Seems there is a bug in reporting the sample offset at the last packet
				// there I added the condition (readCount == 0) to by-pass the bug
				ASSERT(readCount == 0 || stb_vorbis_get_sample_offset(vorbis) == currentSamplePos);
			}

			if(audioBufBegin != currentSamplePos)
				buffer->commitWriteForRange(audioBufBegin, currentSamplePos);
		}

		ringBuffer.collectUnusedSpace();
	}

	return reSchedule();

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

bool loadOgg(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".ogg")) return false;

	TaskPool* taskPool = mgr->taskPool;

	AudioBuffer* buffer = dynamic_cast<AudioBuffer*>(resource);

	OggLoader* loaderTask = new OggLoader(buffer, mgr);

	buffer->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());					// Load meta data
	buffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, buffer->taskReady, ~taskPool->mainThreadId());	// All load completes

	return true;
}

}	// namespace Loader
