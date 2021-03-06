#include "pch.h"
#include "audiobuffer.h"
#include "audioloader.h"
#include "../common.h"
#include "../../roar/base/roFileSystem.h"
#include "../../roar/base/roLog.h"
#include "../../roar/base/roTypeCast.h"
#include "../../thirdParty/libmpg/mpg123.h"

#ifdef RHINOCA_VC
#	pragma comment(lib, "libmpg123")
#endif

using namespace ro;
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
		, mpgRet(MPG123_OK)
	{
		mpg = mpg123_new(NULL, NULL);
		roAssert(mpg);
//		mpg123_param(mpg, MPG123_VERBOSE, 4, 0);
		roVerify(mpg123_param(mpg, MPG123_FLAGS, MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS, 0) == MPG123_OK);

		// Let the seek index auto-grow and contain an entry for every frame
		roVerify(mpg123_param(mpg, MPG123_INDEX_SIZE, -1, 0) == MPG123_OK);

		roVerify(mpg123_open_feed(mpg) == MPG123_OK);

		memset(&format, 0, sizeof(format));
	}

	~Mp3Loader()
	{
		if(stream) fileSystem.closeFile(stream);
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
	int mpgRet;
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
	Status st;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) st = fileSystem.openFile(buffer->uri(), stream);
	if(!st) {
		roLog("error", "Mp3Loader: Fail to open file '%s', reason: %s\n", buffer->uri().c_str(), st.c_str());
		goto Abort;
	}

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	// Load header
	rhbyte buf[_dataChunkSize];
	roUint64 readCount = 0;
	st = fileSystem.read(stream, buf, _dataChunkSize, readCount);
	if(!st) goto Abort;

	int ret = mpg123_decode(mpg, buf, num_cast<size_t>(readCount), NULL, 0, NULL);

	if(ret == MPG123_NEED_MORE) {
		return reSchedule();
	}
	if(ret == MPG123_NEW_FORMAT) {
		long rate;
		int channels, encoding;
		roVerify(mpg123_getformat(mpg, &rate, &channels, &encoding) == MPG123_OK);
		roAssert(encoding == MPG123_ENC_SIGNED_16);

		format.channels = channels;
		format.samplesPerSecond = rate;
		format.bitsPerSample = 16;
		format.blockAlignment = channels * sizeof(rhuint16);

		roUint64 size = 0;
		st = fileSystem.size(stream, size);

		// Mpg123 needs to know the file in order to estimate the audio length
		if(st)
			mpg123_set_filesize(mpg, num_cast<off_t>(size));

		// NOTE: This is just an estimation, for accurate result we need to call mpg123_scan()
		// but it need mpg to be opened in a seekable mode.
		off_t length = mpg123_length(mpg);
		format.estimatedSamples = (length != MPG123_ERR) ? length : 0;

		// NOTE: AudioBuffer::setFormat() is thread safe
		buffer->setFormat(format);
		buffer->scratch = this;
		return;
	}

	roAssert(false);

Abort:
	buffer->state = Resource::Aborted;
	buffer->scratch = this;
}

void Mp3Loader::loadData()
{
	Status st;

	if(buffer->state == Resource::Aborted) goto Abort;
	if(!stream) goto Abort;

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	unsigned audioBufBegin, audioBufEnd;

	// Get the requested audio range, and see if seeking is needed
	if(requestQueue.getRequest(audioBufBegin, audioBufEnd))
	{
		const unsigned backupCurPos = mpg123_tell(mpg);

		off_t fileSeekPos;
		off_t resultingOffset = mpg123_feedseek(mpg, audioBufBegin, SEEK_SET, &fileSeekPos);
		if(resultingOffset < 0)
			return reSchedule();

		bool failed = false;

		roUint64 fileSize = 0;
		st = fileSystem.size(stream, fileSize);
		if(!st || fileSeekPos >= fileSize)
			failed = true;

		if(!failed && !fileSystem.seek(stream, fileSeekPos, FileSystem::SeekOrigin_Begin))
			failed = true;

		if(failed) {
			// TODO: We need some way to report any failure of the seek operation to the audio device
			// Roll back the seek position
			roVerify(mpg123_feedseek(mpg, backupCurPos, SEEK_SET, &fileSeekPos) == backupCurPos);
			requestQueue.request(backupCurPos, backupCurPos + format.samplesPerSecond);
			return reSchedule(true);
		}

		audioBufBegin = resultingOffset;

		// Check for IO ready state once again
		if(fileSystem.readWillBlock(stream, _dataChunkSize))
			return reSchedule();
	}

	// Even if the audio device didn't have any request, we try to begin loading some data
	if(audioBufBegin == audioBufEnd && audioBufBegin == 0)
		audioBufEnd = format.samplesPerSecond;

	unsigned bytesToWrite = 0;
	void* bufferData = buffer->getWritePointerForRange(audioBufBegin, audioBufEnd, bytesToWrite);
	roAssert(bufferData && bytesToWrite);

	// Query the size of the mpg123's internal buffer, 
	// if it's large enough, we need not to perform read from the stream
	long mpgInternalSize = 0;
	if(MPG123_OK != mpg123_getstate(mpg, MPG123_BUFFERFILL, &mpgInternalSize, NULL))
		goto Abort;

	// Read from IO stream, if necessary
	rhbyte buf[_dataChunkSize];
	roUint64 readCount = 0;

	if(mpgRet == MPG123_NEED_MORE || mpgInternalSize <= _dataChunkSize) {
		st = fileSystem.read(stream, buf, _dataChunkSize, readCount);
		if(!st) goto Abort;
	}

	// Perform MP3 decoding
	size_t decodeBytes = 0;
	mpgRet = mpg123_decode(mpg, buf, num_cast<size_t>(readCount), (rhbyte*)bufferData, bytesToWrite, &decodeBytes);

	if(mpgRet == MPG123_ERR)
		goto Abort;

	if(mpgRet == MPG123_NEW_FORMAT)
		roLog("error", "Mp3Loader: Changing audio format in the middle of loading is not supported '%s'\n", buffer->uri().c_str());

	if(decodeBytes > 0) {
		currentSamplePos = mpg123_tell(mpg);
		roAssert(currentSamplePos <= audioBufEnd);
		buffer->commitWriteForRange(audioBufBegin, currentSamplePos);
		requestQueue.commit(audioBufBegin, currentSamplePos);
//		printf("mp3 commit %s: %d, %d\n", buffer->uri().c_str(), audioBufBegin, currentSamplePos);
	}
	else if(readCount > 0)
		reSchedule(false);

	// Condition for EOF
	if(mpgRet == MPG123_DONE || (mpgRet == MPG123_NEED_MORE && readCount == 0)) {
		currentSamplePos = mpg123_tell(mpg);
		format.totalSamples = format.estimatedSamples = currentSamplePos;
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
