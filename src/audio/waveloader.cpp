#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"

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
} ;

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
} ;

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
		, state(0)
		, dataChunkSize(0)
		, bufferData(NULL)
		, taskLoadMetaData(0)
	{
	}

	~WaveLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
		rhinoca_free(bufferData);
	}

	void loadDataForRange(unsigned begin, unsigned end);

	void run(TaskPool* taskPool);

	bool loadHeader();
	void commitHeader();
	bool loadData();

	void* stream;
	AudioBuffer* buffer;
	ResourceManager* manager;

	/// Indicate the state of the load:
	/// 0 -> loading header
	/// 1 -> committing header to AudioBuffer
	/// 2 -> loading audio data
	int state;
	unsigned dataChunkSize;
	WaveFormatExtensible format;
	void* bufferData;

	TaskId taskLoadMetaData;
};

void WaveLoader::loadDataForRange(unsigned begin, unsigned end)
{
	// Submit the range to the command queue
}

void WaveLoader::run(TaskPool* taskPool)
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	buffer->scratch = this;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) stream = io_open(rh, buffer->uri(), tId);
	if(!stream) goto Abort;

	switch(state) {
	case 0:
		if(!loadHeader()) goto Abort;
		break;
	case 1:
		commitHeader();
		break;
	case 2:
		if(!loadData()) goto Abort;
		break;
	default:
		ASSERT(false);
		break;
	}

	return;

Abort:
	buffer->scratch = NULL;
	buffer->state = Resource::Aborted;
	delete this;
}

bool WaveLoader::loadHeader()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	while(true) {
		char chunkId[5] = {0};
		rhint32 chunkSize;

		if(!io_ready(stream, sizeof(chunkId) + sizeof(chunkSize) + sizeof(WaveFormatExtensible), tId)) {
			// Re-schedule the load operation
			reSchedule();
			return true;
		}

		if(	io_read(stream, chunkId, 4, tId) != 4 ||
			io_read(stream, &chunkSize, sizeof(rhint32), tId) != sizeof(rhint32))
		{
			print(rh, "WaveLoader: End of file, fail to load header");
			return false;
		}

		if(strcasecmp(chunkId, "RIFF") == 0)
		{
			char format[5] = {0};
			io_read(stream, format, 4, tId);
		}
		else if(strcasecmp(chunkId, "FMT ") == 0)
		{
			io_read(stream, &format, chunkSize, tId);

			// set format data to buffer
/*			buffer->setChannels( fmt.format.channels );
			buffer->setSamplePerSec( fmt.format.samplesPerSec );
			buffer->setAvgBytesPerSec( fmt.format.avgBytesPerSec );
			buffer->setBlockAlign( fmt.format.blockAlign );
			buffer->setBitsPerSample( fmt.format.bitsPerSample );
			buffer->setSize( fmt.format.size );*/
		}
		else if(strcasecmp(chunkId, "DATA") == 0)
		{
			state = 1;
			dataChunkSize = chunkSize;
			return true;
//			mBufferSize = chunkSize;
//			buffer->initBuffers( mBufferSize );
//			mAudioData = buffer->getCurrentBuffer(); //new byte_t[mBufferSize];

//			is->read( reinterpret_cast<char*>(mAudioData), sizeof(char) * mBufferSize );
		}
		else
			io_seek(stream, chunkSize, SEEK_CUR, tId);
	}

	return true;
}

void WaveLoader::commitHeader()
{
	state = 2;

	buffer->frequency = format.format.samplesPerSec;
	buffer->duration = dataChunkSize / format.format.blockAlign;
}

bool WaveLoader::loadData()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!io_ready(stream, dataChunkSize, tId)) {
		reSchedule();
		return true;
	}

	bufferData = rhinoca_malloc(dataChunkSize);

	if(	io_read(stream, bufferData, dataChunkSize, tId) != dataChunkSize)
	{
		print(rh, "WaveLoader: End of file, only partial load of audio data");
		return false;
	}

	buffer->insertSubBuffer(0, buffer->duration, bufferData);
	bufferData = NULL;

	return true;
}

bool loadWave(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".wav")) return false;

	TaskPool* taskPool = mgr->taskPool;

	AudioBuffer* buffer = dynamic_cast<AudioBuffer*>(resource);

	WaveLoader* loaderTask = new WaveLoader(buffer, mgr);

	loaderTask->taskLoadMetaData = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());	// Load meta data
	buffer->taskReady = taskPool->addFinalized(loaderTask, 0, loaderTask->taskLoadMetaData, taskPool->mainThreadId());	// Commit meta data
	buffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, buffer->taskReady, ~taskPool->mainThreadId());	// All load completes

	return true;
}

}	// namespace Loader