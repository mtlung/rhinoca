#include "stb_vorbis.h"

struct RingBuffer
{
	RingBuffer() : _rBufIdx(0), _wBufIdx(1), _rPos(0), _wPos(0) {}

	roByte* write(roSize maxSizeToWrite)
	{
		roAssert(_wPos == _wBuf().size() && "Call commitWrite() for each write()");
		_wBuf().incSize(maxSizeToWrite);
		return &_wBuf()[_wPos];
	}

	void commitWrite(roSize written)
	{
		roAssert(_wPos + written <= _wBuf().size());
		_wPos += written;
		_wBuf().resize(_wPos);
	}

	roByte* read(roSize& maxSizeToRead)
	{
		if(_rBuf().size() - _rPos == 0) {
			roSwap(_rBufIdx, _wBufIdx);
			_rPos = _wPos = 0;
			_wBuf().clear();
		}

		maxSizeToRead = _rBuf().size() - _rPos;
		if(!maxSizeToRead)
			return NULL;

		return &_rBuf()[_rPos];
	}

	void commitRead(roSize read)
	{
		roAssert(_rPos + read <= _rBuf().size());
		_rPos += read;
	}

	Array<roByte>& _rBuf() { return _buffers[_rBufIdx]; }
	Array<roByte>& _wBuf() { return _buffers[_wBufIdx]; }

	roSize _rBufIdx, _wBufIdx;
	roSize _rPos, _wPos;
	Array<roByte> _buffers[2];	/// Two buffers, one for read and one for write
};

struct RingBuffer2
{
	RingBuffer2()
		: readPos(0), writePos(0)
	{}

	roByte* write(roSize maxSizeToWrite)
	{
		roAssert(writePos == buffer.size() && "Call commitWrite() for each write()");
		buffer.incSize(maxSizeToWrite);
		return &buffer[writePos];
	}

	void commitWrite(roSize written)
	{
		roAssert(writePos + written <= buffer.size());
		writePos += written;
		buffer.resize(writePos);
	}

	roByte* read(roSize& maxSizeToRead)
	{
		maxSizeToRead = writePos - readPos;
		return &buffer[readPos];
	}

	void commitRead(roSize read)
	{
		roAssert(readPos + read <= writePos);
		readPos += read;
	}

	void clear()
	{
		buffer.clear();
		readPos = writePos = 0;
	}

	void collectUnusedSpace()
	{
		// Move remaining data at the back to the font space
		roSize sizeToMove = writePos - readPos;
		if(sizeToMove)
			memmove(&buffer[0], &buffer[readPos], sizeToMove);
		buffer.resize(sizeToMove);
		readPos = 0;
		writePos = sizeToMove;
	}

	Array<roByte> buffer;
	roSize readPos, writePos;
};	// RingBuffer

struct RingBuffer3
{
	RingBuffer3()
		: buffer(NULL)
		, readPos(0), writePos(0), endPos(0)
	{}

	~RingBuffer3()
	{
		free(buffer);
	}

	roByte* write(roSize maxSizeToWrite)
	{
		if(writePos + maxSizeToWrite >= endPos) {
			buffer = (roByte*)realloc(buffer, endPos + maxSizeToWrite);
			endPos += maxSizeToWrite;
		}
		return buffer + writePos;
	}

	void commitWrite(roSize written)
	{
		roAssert(writePos + written <= endPos);
		writePos += written;
	}

	roByte* read(roSize& maxSizeToRead)
	{
		maxSizeToRead = writePos - readPos;
		return buffer + readPos;
	}

	void commitRead(roSize read)
	{
		roAssert(readPos + read <= writePos);
		readPos += read;
	}

	void clear()
	{
		readPos = writePos = 0;
	}

	void collectUnusedSpace()
	{
		// Move remaining data at the back to the font space
		roSize sizeToMove = writePos - readPos;
		memcpy(buffer, buffer + readPos, sizeToMove);
		readPos = 0;
		writePos = sizeToMove;
	}

	roByte* buffer;
	roSize readPos, writePos, endPos;
};	// RingBuffer3

struct OggLoader : public AudioLoader
{
	OggLoader(AudioBuffer* b, ResourceManager* mgr)
		: AudioLoader(b, mgr)
		, vorbis(NULL)
		, curPcmPos(0)
		, nextFun(&OggLoader::loadHeader)
	{
	}

	~OggLoader()
	{
		if(vorbis) stb_vorbis_close(vorbis);
		if(stream) fileSystem.closeFile(stream);
	}

	override void run(TaskPool* taskPool);

	void loadHeader(TaskPool* taskPool);
	void commitHeader(TaskPool* taskPool);
	void checkRequest(TaskPool* taskPool);
	void processRequest(TaskPool* taskPool);
	void commitData(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	stb_vorbis* vorbis;
	stb_vorbis_info vorbisInfo;

	unsigned curPcmPos;
	RingBuffer3 ringBuffer;
	Array<roByte> pcmData;
	Array<unsigned> pcmRequestShadow;

	void (OggLoader::*nextFun)(TaskPool*);
};

void OggLoader::run(TaskPool* taskPool)
{
	if(audioBuffer->state == Resource::Aborted || !taskPool->keepRun())
		nextFun = &OggLoader::abort;

	(this->*nextFun)(taskPool);
}

//static const unsigned _dataChunkSize = 1024 * 16;

void OggLoader::loadHeader(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(audioBuffer->uri(), stream);
	if(!st) roEXCP_THROW;

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	// Read from stream and put to ring buffer
	roByte* buf = ringBuffer.write(_dataChunkSize);
	roUint64 readCount = 0;
	st = fileSystem.read(stream, buf, _dataChunkSize, readCount);
	if(!st) roEXCP_THROW;
	ringBuffer.commitWrite(num_cast<roSize>(readCount));

	// Read from ring buffer and put to vorbis
	roSize bytesRead = num_cast<roSize>(readCount);
	buf = ringBuffer.read(bytesRead);
	int error, byteUsed = 0;
	vorbis = stb_vorbis_open_pushdata(buf, bytesRead, &byteUsed, &error, NULL);

	if(error == VORBIS_need_more_data)
		return reSchedule();
	else if(!vorbis || error != 0)
		roEXCP_THROW;

	// Reading of header success
	ringBuffer.commitRead(byteUsed);
	ringBuffer.collectUnusedSpace();
	vorbisInfo = stb_vorbis_get_info(vorbis);

	format.channels = vorbisInfo.channels;
	format.samplesPerSecond = vorbisInfo.sample_rate;
	format.bitsPerSample = 16;
	format.blockAlignment = vorbisInfo.channels * 2;
	format.totalSamples = format.estimatedSamples = 0;

	nextFun = &OggLoader::commitHeader;

roEXCP_CATCH
	roLog("error", "WaveLoader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &OggLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void OggLoader::commitHeader(TaskPool* taskPool)
{
	audioBuffer->format = format;
	nextFun = &OggLoader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

void OggLoader::checkRequest(TaskPool* taskPool)
{
	pcmRequestShadow = pcmRequest;

	if(pcmRequestShadow.isEmpty())
		return reSchedule(true, taskPool->mainThreadId());
	else {
		nextFun = &OggLoader::processRequest;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
}

void OggLoader::processRequest(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	Status st;

roEXCP_TRY
	if(!stream) { st = Status::pointer_is_null; roEXCP_THROW; }

	if(fileSystem.readWillBlock(stream, _dataChunkSize))
		return reSchedule();

	roAssert(!pcmRequestShadow.isEmpty());

	unsigned requestPcmPos = pcmRequestShadow.front();

	// Read from stream and put to ring buffer
	roByte* buf = ringBuffer.write(_dataChunkSize);
	roUint64 bytesRead = 0;
	st = fileSystem.read(stream, buf, _dataChunkSize, bytesRead);
	if(!st) roEXCP_THROW;
	roSize readCount = num_cast<unsigned>(bytesRead);
	ringBuffer.commitWrite(readCount);

//	if(readCount == 0) {	// EOF
//		format.totalSamples = format.estimatedSamples = curPcmPos;
//		return;
//	}

	// Read from ring buffer and put to vorbis
	float** outputs = NULL;
	int sampleCount = 0;
	int byteUsed = 0;
	const int numChannels = vorbisInfo.channels <= 2 ? vorbisInfo.channels : 2;

	pcmData.clear();

	// Loop until we fill up the allocated audio buffer
	while(true)
	{
		buf = ringBuffer.read(readCount);

		byteUsed = stb_vorbis_decode_frame_pushdata(vorbis, buf, readCount, NULL, &outputs, &sampleCount);

		// Not enough data in the buffer to construct a single frame, skip to next turn
		if(!byteUsed)
			break;

		ringBuffer.commitRead(byteUsed);

		if(!sampleCount)
			continue;

		// By default stb_vorbis load data as float, we need to convert to uint16 before submitting to audio device
		roSize offset = pcmData.size();
		pcmData.incSize(sampleCount * numChannels * 2);
		stb_vorbis_channels_short_interleaved(numChannels, (short*)&pcmData[offset], vorbisInfo.channels, outputs, 0, sampleCount);

		curPcmPos += sampleCount;
	}

	ringBuffer.collectUnusedSpace();

	// Condition for EOF
/*	if(mpgRet == MPG123_DONE || (mpgRet == MPG123_NEED_MORE && st == Status::file_ended)) {
		curPcmPos = mpg123_tell(mpg);
		format.totalSamples = format.estimatedSamples = curPcmPos;
	}

	roAssert(decodeBytes <= pcmData.sizeInByte());
	pcmData.resize(decodeBytes);*/
	nextFun = &OggLoader::commitData;

roEXCP_CATCH
	roLog("error", "OggLoader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &OggLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void OggLoader::commitData(TaskPool* taskPool)
{
	audioBuffer->format = format;

	unsigned pcmPos = pcmRequestShadow.front();

	audioBuffer->addSubBuffer(pcmPos, pcmData.bytePtr(), pcmData.sizeInByte());

	// Remove the processed entry form the request list
	pcmRequest.removeByKey(pcmPos);

	nextFun = &OggLoader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

void OggLoader::abort(TaskPool* taskPool)
{
	audioBuffer->state = Resource::Aborted;
	roAssert(audioBuffer->loader == this);
	audioBuffer->loader = NULL;
	delete this;
}

Resource* resourceCreateOgg(ResourceManager* mgr, const char* uri)
{
	return new AudioBuffer(uri);
}

bool resourceLoadOgg(ResourceManager* mgr, Resource* resource)
{
	AudioBuffer* audioBuffer = dynamic_cast<AudioBuffer*>(resource);
	if(!audioBuffer)
		return false;

	TaskPool* taskPool = mgr->taskPool;
	OggLoader* loaderTask = new OggLoader(audioBuffer, mgr);
	audioBuffer->taskReady = audioBuffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingOgg(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".ogg"))
		return false;

	createFunc = resourceCreateOgg;
	loadFunc = resourceLoadOgg;
	return true;
}
