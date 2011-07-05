#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "../../thirdParty/libmpg/mpg123.h"

#ifdef RHINOCA_VC
#	pragma comment(lib, "libmpg123")
#endif

using namespace Audio;

namespace Loader {

Resource* createMp3(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".mp3"))
		return new AudioBuffer(uri);
	return NULL;
}

struct MpgInitializer {
	MpgInitializer() { mpg123_init(); }
	~MpgInitializer() { mpg123_exit(); }
} _mpgInitializer;

class Mp3Loader : public AudioLoader
{
public:
	Mp3Loader(AudioBuffer* buf, ResourceManager* mgr)
		: stream(NULL)
		, buffer(buf)
		, manager(mgr)
		, preloadAll(false)
		, headerLoaded(false)
		, currentSamplePos(0)
	{
		mpg = mpg123_new(NULL, NULL);
		ASSERT(mpg);
//		mpg123_param(mpg, MPG123_VERBOSE, 4, 0);
		VERIFY(mpg123_param(mpg, MPG123_FLAGS, MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS, 0) == MPG123_OK);

		// Let the seek index auto-grow and contain an entry for every frame
		VERIFY(mpg123_param(mpg, MPG123_INDEX_SIZE, -1, 0) == MPG123_OK);

		VERIFY(mpg123_open_feed(mpg) == MPG123_OK);

		memset(&format, 0, sizeof(format));
	}

	~Mp3Loader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
		if(mpg) mpg123_delete(mpg);
	}

	void loadDataForRange(unsigned begin, unsigned end);

	void run(TaskPool* taskPool);

	void loadHeader();
	void loadData();

	void* stream;
	AudioBuffer* buffer;
	ResourceManager* manager;

	bool preloadAll;	// If not preloadAll, data will be loaded on request by audio device.
	bool headerLoaded;
	AudioBuffer::Format format;

	unsigned currentSamplePos;

	mpg123_handle* mpg;
};

void Mp3Loader::loadDataForRange(unsigned begin, unsigned end)
{
	requestQueue.request(begin, end);

	TaskPool* taskPool = manager->taskPool;
	taskPool->resume(buffer->taskLoaded);
}

void Mp3Loader::run(TaskPool* taskPool)
{
	if(!buffer->scratch)
		loadHeader();
	else
		loadData();
}

static const unsigned _dataChunkSize = 1024 * 16;

void Mp3Loader::loadHeader()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = io_open(rh, buffer->uri(), tId);
	if(!stream) {
		print(rh, "Mp3Loader: Fail to open file '%s'\n", buffer->uri().c_str());
		goto Abort;
	}

	if(!io_ready(stream, _dataChunkSize, tId))
		return reSchedule();

	// Load header
	rhbyte buf[_dataChunkSize];
	unsigned readCount = (unsigned)io_read(stream, buf, _dataChunkSize, tId);

	int ret = mpg123_decode(mpg, buf, readCount, NULL, 0, NULL);

	if(ret == MPG123_NEED_MORE) {
		return reSchedule();
	}
	if(ret == MPG123_NEW_FORMAT) {
		long rate;
		int channels, encoding;
		VERIFY(mpg123_getformat(mpg, &rate, &channels, &encoding) == MPG123_OK);
		ASSERT(encoding == MPG123_ENC_SIGNED_16);

		format.channels = channels;
		format.samplesPerSecond = rate;
		format.bitsPerSample = 16;
		format.blockAlignment = channels * sizeof(rhuint16);

		off_t length = mpg123_length(mpg);
		format.totalSamples = (length != MPG123_ERR) ? length : 0;

		// NOTE: AudioBuffer::setFormat() is thread safe
		buffer->setFormat(format);
		buffer->scratch = this;
		return;
	}

	ASSERT(false);

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

void Mp3Loader::loadData()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) goto Abort;

	if(!io_ready(stream, _dataChunkSize, tId))
		return reSchedule();

	unsigned audioBufBegin, audioBufEnd;
	const bool seek = requestQueue.getRequest(audioBufBegin, audioBufEnd);
	// TODO: Handle seeking

	// Even if the audio device didn't have any request, we try to begin loading some data
	if(audioBufBegin == audioBufEnd && audioBufBegin == 0)
		audioBufEnd = format.samplesPerSecond;

	unsigned bytesToWrite = 0;
	void* bufferData = buffer->getWritePointerForRange(audioBufBegin, audioBufEnd, bytesToWrite);
	ASSERT(bufferData && bytesToWrite);

	// Read from IO stream
	rhbyte buf[_dataChunkSize];
	unsigned readCount = (unsigned)io_read(stream, buf, _dataChunkSize, tId);

	// Perform MP3 decoding
	// TODO: Current implementation may lead to unbounded increasing of mpg123
	// internal buffer. I already submitted a feature request to query the size
	// of output I can get without feeding new data.
	// May be implement a algorithm to adjust _dataChunkSize dynamically
	unsigned decodeBytes = 0;
	int ret = mpg123_decode(mpg, buf, readCount, (rhbyte*)bufferData, bytesToWrite, &decodeBytes);

	if(ret == MPG123_ERR)
		goto Abort;

	if(ret == MPG123_NEW_FORMAT)
		print(rh, "Mp3Loader: Changing audio format in the middle of loading is not supported '%s'\n", buffer->uri().c_str());

	if(bytesToWrite > 0) {
		currentSamplePos = mpg123_tell(mpg);
		ASSERT(currentSamplePos <= audioBufEnd);
		buffer->commitWriteForRange(audioBufBegin, currentSamplePos);
		requestQueue.commit(audioBufBegin, currentSamplePos);
//		printf("mp3 commit  : %d, %d\n", audioBufBegin, currentSamplePos);
	}

	// Condition for EOF
	if(ret == MPG123_DONE || (ret == MPG123_NEED_MORE && readCount == 0)) {
		currentSamplePos = mpg123_tell(mpg);
		format.totalSamples = currentSamplePos;
		buffer->setFormat(format);
		buffer->state = Resource::Loaded;

		return;
	}

	return reSchedule(true);

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

bool loadMp3(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".mp3")) return false;

	TaskPool* taskPool = mgr->taskPool;

	AudioBuffer* buffer = dynamic_cast<AudioBuffer*>(resource);

	Mp3Loader* loaderTask = new Mp3Loader(buffer, mgr);

	buffer->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());					// Load meta data
	buffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, buffer->taskReady, ~taskPool->mainThreadId());	// All load completes

	return true;
}

}	// namespace Loader
