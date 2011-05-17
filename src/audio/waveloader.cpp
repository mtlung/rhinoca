#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include <string.h>

namespace Loader {

struct WaveFormatEx
{
	rhint16 formatTag;
	rhint16 channels;
	rhint32 samplesPerSec;
	rhint32 avgBytesPerSec;
	rhint16 blockAlign;
	rhint16 bitsPerSample;
	rhint16 size;
};

struct WaveFormatExtensible
{
	WaveFormatEx format;
	union
	{
		rhint16 wValidBitsPerSample;
		rhint16 wSamplesPerBlock;
		rhint16 wReserved;
	} samples;
	rhint32 channelMask;
	rhbyte subFormat[16];
};

Resource* createWave(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".wav"))
		return new AudioBuffer(uri);
	return NULL;
}

class WaveLoader : public AudioLoader
{
public:
	WaveLoader(AudioBuffer* buf, ResourceManager* mgr)
		: stream(NULL)
		, buffer(buf)
		, manager(mgr)
		, headerLoaded(false)
		, dataChunkSize(0)
		, taskLoadMetaData(0)
	{
	}

	~WaveLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
	}

	void loadDataForRange(unsigned begin, unsigned end);

	void run(TaskPool* taskPool);

	void loadHeader();
	void loadData();

	void* stream;
	AudioBuffer* buffer;
	ResourceManager* manager;

	bool headerLoaded;
	unsigned dataChunkSize;
	WaveFormatExtensible format;

	TaskId taskLoadMetaData;
};

void WaveLoader::loadDataForRange(unsigned begin, unsigned end)
{
	// Submit the range to the command queue
}

void WaveLoader::run(TaskPool* taskPool)
{
	if(!buffer->scratch)
		loadHeader();
	else
		loadData();
}

void WaveLoader::loadHeader()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = io_open(rh, buffer->uri(), tId);
	if(!stream) {
		print(rh, "WaveLoader: Fail to open file '%s'\n", buffer->uri().c_str());
		goto Abort;
	}

	while(true) {
		char chunkId[5] = {0};
		rhint32 chunkSize;

		if(!io_ready(stream, sizeof(chunkId) + sizeof(chunkSize) + sizeof(WaveFormatExtensible), tId))
			return reSchedule();

		if(	io_read(stream, chunkId, 4, tId) != 4 ||
			io_read(stream, &chunkSize, sizeof(rhint32), tId) != sizeof(rhint32))
		{
			print(rh, "WaveLoader: End of file, fail to load header");
			goto Abort;
		}

		if(strcasecmp(chunkId, "RIFF") == 0)
		{
			char format[5] = {0};
			io_read(stream, format, 4, tId);
		}
		else if(strcasecmp(chunkId, "FMT ") == 0)
		{
			io_read(stream, &format, chunkSize, tId);

		}
		else if(strcasecmp(chunkId, "DATA") == 0)
		{
			dataChunkSize = chunkSize;

			AudioBuffer::Format f = {
				format.format.channels,
				format.format.samplesPerSec,
				format.format.bitsPerSample,
				format.format.blockAlign,
				dataChunkSize / format.format.blockAlign
			};

			// NOTE: AudioBuffer::setFormat() is thread safe
			buffer->setFormat(f);

			break;
		}
		else
			VERIFY(io_seek(stream, chunkSize, SEEK_CUR, tId));
	}

	buffer->scratch = this;
	return;

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

void WaveLoader::loadData()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;
	void* bufferData = NULL;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) goto Abort;

	if(!io_ready(stream, dataChunkSize, tId))
		return reSchedule();

	bufferData = buffer->getWritePointerForRange(0, dataChunkSize / format.format.blockAlign);

	if(io_read(stream, bufferData, dataChunkSize, tId) != dataChunkSize)
	{
		print(rh, "WaveLoader: End of file, only partial load of audio data");
		goto Abort;
	}

	buffer->commitWriteForRange(0, dataChunkSize / format.format.blockAlign);
	return;

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

bool loadWave(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".wav")) return false;

	TaskPool* taskPool = mgr->taskPool;

	AudioBuffer* buffer = dynamic_cast<AudioBuffer*>(resource);

	WaveLoader* loaderTask = new WaveLoader(buffer, mgr);

	buffer->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());					// Load meta data
	buffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, buffer->taskReady, ~taskPool->mainThreadId());	// All load completes

	return true;
}

}	// namespace Loader
