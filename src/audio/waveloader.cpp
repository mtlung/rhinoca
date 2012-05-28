#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "../common.h"
#include "../../roar/base/roFileSystem.h"
#include "../../roar/base/roLog.h"

namespace Loader {

using namespace ro;
using namespace Audio;

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
	{
	}

	~WaveLoader()
	{
		if(stream) fileSystem.closeFile(stream);
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
	Status st;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) st = fileSystem.openFile(buffer->uri(), stream);
	if(!st) {
		roLog("error", "WaveLoader: Fail to open file '%s', reason: %s\n", buffer->uri().c_str(), st.c_str());
		goto Abort;
	}

	while(true) {
		char chunkId[5] = {0};
		rhint32 chunkSize;

		if(fileSystem.readWillBlock(stream, sizeof(chunkId) + sizeof(chunkSize) + sizeof(WaveFormatExtensible)))
			return reSchedule();

		Status st = fileSystem.atomicRead(stream, chunkId, 4);
		if(!st) goto Abort;

		st = fileSystem.atomicRead(stream, &chunkSize, sizeof(rhint32));
		if(!st) goto Abort;

		if(roStrCaseCmp(chunkId, "RIFF") == 0)
		{
			char format[5] = {0};
			st = fileSystem.atomicRead(stream, format, 4);
			if(!st) goto Abort;
		}
		else if(roStrCaseCmp(chunkId, "FMT ") == 0)
		{
			st = fileSystem.atomicRead(stream, &format, chunkSize);
			if(!st) goto Abort;
		}
		else if(roStrCaseCmp(chunkId, "DATA") == 0)
		{
			dataChunkSize = chunkSize;

			AudioBuffer::Format f = {
				format.format.channels,
				format.format.samplesPerSec,
				format.format.bitsPerSample,
				format.format.blockAlign,
				dataChunkSize / format.format.blockAlign,
				dataChunkSize / format.format.blockAlign
			};

			// NOTE: AudioBuffer::setFormat() is thread safe
			buffer->setFormat(f);

			break;
		}
		else
			roVerify(fileSystem.seek(stream, chunkSize, FileSystem::SeekOrigin_Current));
	}

	buffer->scratch = this;
	return;

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

void WaveLoader::loadData()
{
	void* bufferData = NULL;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) goto Abort;

	if(fileSystem.readWillBlock(stream, dataChunkSize))
		return reSchedule();

	{	unsigned end = dataChunkSize / format.format.blockAlign;
		unsigned bytesToWrite = 0;
		bufferData = buffer->getWritePointerForRange(0, end, bytesToWrite);

		roAssert(bytesToWrite == dataChunkSize);

		Status st = fileSystem.atomicRead(stream, bufferData, dataChunkSize);
		if(!st) goto Abort;

		buffer->commitWriteForRange(0, dataChunkSize / format.format.blockAlign);
		buffer->state = Resource::Loaded;
	}

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
