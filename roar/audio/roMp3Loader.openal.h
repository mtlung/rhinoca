#include "../../thirdParty/libmpg/mpg123.h"
#include <stdio.h>	// For SEEK_SET macro

#if roCOMPILER_VC
#	pragma comment(lib, "libmpg123")
#endif

struct MpgInitializer {
	MpgInitializer() { mpg123_init(); }
	~MpgInitializer() { mpg123_exit(); }
} _mpgInitializer;

struct Mp3Loader : public AudioLoader
{
	Mp3Loader(AudioBuffer* b, ResourceManager* mgr)
		: AudioLoader(b, mgr)
		, curPcmPos(0)
		, nextFun(&Mp3Loader::loadHeader)
	{
		mpg = mpg123_new(NULL, NULL);
		roAssert(mpg);
//		mpg123_param(mpg, MPG123_VERBOSE, 4, 0);
		roVerify(mpg123_param(mpg, MPG123_FLAGS, MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS, 0) == MPG123_OK);

		// Let the seek index auto-grow and contain an entry for every frame
		roVerify(mpg123_param(mpg, MPG123_INDEX_SIZE, -1, 0) == MPG123_OK);

		roVerify(mpg123_open_feed(mpg) == MPG123_OK);
	}

	~Mp3Loader()
	{
		if(mpg) mpg123_delete(mpg);
	}

	override void run(TaskPool* taskPool);

	void loadHeader(TaskPool* taskPool);
	void commitHeader(TaskPool* taskPool);
	void checkRequest(TaskPool* taskPool);
	void processRequest(TaskPool* taskPool);
	void commitData(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	mpg123_handle* mpg;
	unsigned curPcmPos;
	Array<roByte> pcmData;
	Array<unsigned> pcmRequestShadow;

	void (Mp3Loader::*nextFun)(TaskPool*);
};

void Mp3Loader::run(TaskPool* taskPool)
{
	if(audioBuffer->state == Resource::Aborted || !taskPool->keepRun())
		nextFun = &Mp3Loader::abort;

	(this->*nextFun)(taskPool);
}

static const unsigned _dataChunkSize = 1024 * 16;

void Mp3Loader::loadHeader(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(audioBuffer->uri(), stream);
	if(!st) roEXCP_THROW;

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	// Load header
	roByte buf[_dataChunkSize];
	roUint64 readCount = 0;
	st = fileSystem.read(stream, buf, _dataChunkSize, readCount);
	if(!st) roEXCP_THROW;

	int ret = mpg123_decode(mpg, buf, num_cast<size_t>(readCount), NULL, 0, NULL);

	if(ret == MPG123_NEED_MORE)
		return reSchedule();

	if(ret == MPG123_NEW_FORMAT) {
		long rate;
		int channels, encoding;
		roVerify(mpg123_getformat(mpg, &rate, &channels, &encoding) == MPG123_OK);
		roAssert(encoding == MPG123_ENC_SIGNED_16);

		format.channels = channels;
		format.samplesPerSecond = rate;
		format.bitsPerSample = 16;
		format.blockAlignment = channels * sizeof(roUint16);

		// Mpg123 needs to know the file in order to estimate the audio length
		roUint64 size = 0;
		if(st = fileSystem.size(stream, size))
			mpg123_set_filesize(mpg, num_cast<off_t>(size));

		// NOTE: This is just an estimation, for accurate result we need to call mpg123_scan()
		// but it need mpg to be opened in a seekable mode.
		off_t length = mpg123_length(mpg);
		format.estimatedSamples = (length != MPG123_ERR) ? length : 0;

		nextFun = &Mp3Loader::commitHeader;
		return reSchedule(false, taskPool->mainThreadId());
	}

	roAssert(false);

roEXCP_CATCH
	roLog("error", "WaveLoader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &Mp3Loader::abort;

roEXCP_END
	nextFun = &Mp3Loader::abort;
	return reSchedule(false, taskPool->mainThreadId());
}

void Mp3Loader::commitHeader(TaskPool* taskPool)
{
	audioBuffer->format = format;
	nextFun = &Mp3Loader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

void Mp3Loader::checkRequest(TaskPool* taskPool)
{
	pcmRequestShadow = pcmRequest;

	if(pcmRequestShadow.isEmpty())
		return reSchedule(true, taskPool->mainThreadId());
	else {
		nextFun = &Mp3Loader::processRequest;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
}

void Mp3Loader::processRequest(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) { st = Status::pointer_is_null; roEXCP_THROW; }

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	roAssert(!pcmRequestShadow.isEmpty());

	// Perform seek if necessary
	if(curPcmPos != pcmRequestShadow.front()) {
		off_t fileSeekPos = 0;
		off_t resultingOffset = mpg123_feedseek(mpg, pcmRequestShadow.front(), SEEK_SET, &fileSeekPos);
		if(resultingOffset < 0)
			roEXCP_THROW;

		st = fileSystem.seek(stream, fileSeekPos, FileSystem::SeekOrigin_Begin);
		if(!st) roEXCP_THROW;

		// Check for IO ready state once again
		if(fileSystem.readWillBlock(stream, _dataChunkSize))
			return reSchedule();

		curPcmPos = resultingOffset;
	}

	// Query the size of the mpg123's internal buffer, 
	// if it's large enough, we need not to perform read from the stream
	long mpgInternalSize = 0;
	int mpgRet = MPG123_OK;
	if(MPG123_OK != mpg123_getstate(mpg, MPG123_BUFFERFILL, &mpgInternalSize, NULL))
		roEXCP_THROW;

	// Read from IO stream, if necessary
	roByte buf[_dataChunkSize];
	roUint64 readCount = 0;

	if(mpgRet == MPG123_NEED_MORE || mpgInternalSize <= _dataChunkSize) {
		st = fileSystem.read(stream, buf, _dataChunkSize, readCount);
		if(!st) roEXCP_THROW;
	}

	// Perform MP3 decoding
	pcmData.resize(1024 * 16);
	size_t decodeBytes = 0;
	mpgRet = mpg123_decode(mpg, buf, num_cast<size_t>(readCount), pcmData.typedPtr(), pcmData.sizeInByte(), &decodeBytes);

	if(mpgRet == MPG123_ERR)
		roEXCP_THROW;

	if(mpgRet == MPG123_NEW_FORMAT) {
		roLog("error", "Mp3Loader: Changing audio format in the middle of loading is not supported '%s'\n", audioBuffer->uri().c_str());
		roEXCP_THROW;
	}

	if(decodeBytes > 0) {
		curPcmPos = mpg123_tell(mpg);
	}
	else if(readCount > 0)
		reSchedule(false);

	// Condition for EOF
	if(mpgRet == MPG123_DONE || (mpgRet == MPG123_NEED_MORE && readCount == 0)) {
		curPcmPos = mpg123_tell(mpg);
		format.totalSamples = format.estimatedSamples = curPcmPos;
//		nextFun = &Mp3Loader::checkRequest;
//		return reSchedule(false, taskPool->mainThreadId());
	}

	roAssert(decodeBytes <= pcmData.sizeInByte());
	pcmData.resize(decodeBytes);

roEXCP_CATCH
	roLog("error", "Mp3Loader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &Mp3Loader::abort;

roEXCP_END
	nextFun = &Mp3Loader::commitData;
	return reSchedule(false, taskPool->mainThreadId());
}

void Mp3Loader::commitData(TaskPool* taskPool)
{
	audioBuffer->format = format;

	unsigned pcmPos = pcmRequestShadow.front();
	audioBuffer->addSubBuffer(pcmPos, pcmData.bytePtr(), pcmData.sizeInByte());

	// Remove the processed entry form the request list
	pcmRequest.removeByKey(pcmPos);

	nextFun = &Mp3Loader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

void Mp3Loader::abort(TaskPool* taskPool)
{
	audioBuffer->state = Resource::Aborted;
	roAssert(audioBuffer->loader == this);
	audioBuffer->loader = NULL;
	delete this;
}

Resource* resourceCreateMp3(ResourceManager* mgr, const char* uri)
{
	return new AudioBuffer(uri);
}

bool resourceLoadMp3(ResourceManager* mgr, Resource* resource)
{
	AudioBuffer* audioBuffer = dynamic_cast<AudioBuffer*>(resource);
	if(!audioBuffer)
		return false;

	TaskPool* taskPool = mgr->taskPool;
	Mp3Loader* loaderTask = new Mp3Loader(audioBuffer, mgr);
	audioBuffer->taskReady = audioBuffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingMp3(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".mp3"))
		return false;

	createFunc = resourceCreateMp3;
	loadFunc = resourceLoadMp3;
	return true;
}
