#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "../common.h"
#include "../rhlog.h"
#include "../../roar/base/roFileSystem.h"

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
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = fileSystem.openFile(buffer->uri());
	if(!stream) {
		rhLog("error", "WaveLoader: Fail to open file '%s'\n", buffer->uri().c_str());
		goto Abort;
	}

	while(true) {
		char chunkId[5] = {0};
		rhint32 chunkSize;

		if(!fileSystem.readReady(stream, sizeof(chunkId) + sizeof(chunkSize) + sizeof(WaveFormatExtensible)))
			return reSchedule();

		if(	fileSystem.read(stream, chunkId, 4) != 4 ||
			fileSystem.read(stream, &chunkSize, sizeof(rhint32)) != sizeof(rhint32))
		{
			rhLog("error", "WaveLoader: End of file, fail to load header\n");
			goto Abort;
		}

		if(strcasecmp(chunkId, "RIFF") == 0)
		{
			char format[5] = {0};
			RHVERIFY(fileSystem.read(stream, format, 4) == 4);
		}
		else if(strcasecmp(chunkId, "FMT ") == 0)
		{
			RHVERIFY(fileSystem.read(stream, &format, chunkSize) == chunkSize);
		}
		else if(strcasecmp(chunkId, "DATA") == 0)
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
			RHVERIFY(fileSystem.seek(stream, chunkSize, FileSystem::SeekOrigin_Current));
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

	if(!fileSystem.readReady(stream, dataChunkSize))
		return reSchedule();

	{	unsigned end = dataChunkSize / format.format.blockAlign;
		unsigned bytesToWrite = 0;
		bufferData = buffer->getWritePointerForRange(0, end, bytesToWrite);

		RHASSERT(bytesToWrite == dataChunkSize);

		if(fileSystem.read(stream, bufferData, dataChunkSize) != dataChunkSize)
		{
			rhLog("error", "WaveLoader: End of file, only partial load of audio data\n");
			goto Abort;
		}

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
