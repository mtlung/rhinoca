struct WaveFormatEx
{
	roInt16 formatTag;
	roInt16 channels;
	roInt32 samplesPerSec;
	roInt32 avgBytesPerSec;
	roInt16 blockAlign;
	roInt16 bitsPerSample;
	roInt16 size;
};

struct WaveFormatExtensible
{
	WaveFormatEx format;
	union
	{
		roInt16 wValidBitsPerSample;
		roInt16 wSamplesPerBlock;
		roInt16 wReserved;
	} samples;
	roInt32 channelMask;
	roByte subFormat[16];
};

struct WaveLoader : public AudioLoader
{
	WaveLoader(AudioBuffer* b, ResourceManager* mgr)
		: AudioLoader(b, mgr)
		, nextFun(&WaveLoader::loadHeader)
	{}

	~WaveLoader()
	{
		if(stream) fileSystem.closeFile(stream);
	}

	override void run(TaskPool* taskPool);

	void loadHeader(TaskPool* taskPool);
	void commitHeader(TaskPool* taskPool);
	void loadData(TaskPool* taskPool);
	void commitData(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	Array<roByte> pcmData;
	WaveFormatExtensible formatEx;

	void (WaveLoader::*nextFun)(TaskPool*);
};

void WaveLoader::run(TaskPool* taskPool)
{
	if(audioBuffer->state == Resource::Aborted || !taskPool->keepRun())
		nextFun = &WaveLoader::abort;

	(this->*nextFun)(taskPool);
}

void WaveLoader::loadHeader(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(audioBuffer->uri(), stream);
	if(!st) roEXCP_THROW;

	while(true) {
		char chunkId[5] = {0};
		roInt32 chunkSize;

		if(fileSystem.readWillBlock(stream, sizeof(chunkId) + sizeof(chunkSize) + sizeof(WaveFormatExtensible)))
			return reSchedule();

		Status st = fileSystem.atomicRead(stream, chunkId, 4);
		if(!st) roEXCP_THROW;

		st = fileSystem.atomicRead(stream, &chunkSize, sizeof(roInt32));
		if(!st) roEXCP_THROW;

		if(roStrCaseCmp(chunkId, "RIFF") == 0)
		{
			char format_[5] = {0};
			st = fileSystem.atomicRead(stream, format_, 4);
			if(!st) roEXCP_THROW;
		}
		else if(roStrCaseCmp(chunkId, "FMT ") == 0)
		{
			st = fileSystem.atomicRead(stream, &formatEx, chunkSize);
			if(!st) roEXCP_THROW;
		}
		else if(roStrCaseCmp(chunkId, "DATA") == 0)
		{
			pcmData.resize(chunkSize);

			AudioBuffer::Format f = {
				formatEx.format.channels,
				formatEx.format.samplesPerSec,
				formatEx.format.bitsPerSample,
				formatEx.format.blockAlign,
				chunkSize / formatEx.format.blockAlign,
				chunkSize / formatEx.format.blockAlign
			};

			format = f;
			nextFun = &WaveLoader::commitHeader;

			break;
		}
		else
			roVerify(fileSystem.seek(stream, chunkSize, FileSystem::SeekOrigin_Current));
	}

roEXCP_CATCH
	roLog("error", "WaveLoader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &WaveLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void WaveLoader::commitHeader(TaskPool* taskPool)
{
	audioBuffer->format = format;
	nextFun = &WaveLoader::loadData;
	return reSchedule(false, ~taskPool->mainThreadId());
}

void WaveLoader::loadData(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope("WaveLoader::loadData");

	Status st;

roEXCP_TRY
	if(!stream) { st = Status::pointer_is_null; roEXCP_THROW; }

	if(fileSystem.readWillBlock(stream, pcmData.size()))
		return reSchedule(false, ~taskPool->mainThreadId());

	Status st = fileSystem.atomicRead(stream, pcmData.bytePtr(), pcmData.size());
	if(!st) roEXCP_THROW;

	nextFun = &WaveLoader::commitData;

roEXCP_CATCH
	roLog("error", "WaveLoader: Fail to load '%s', reason: %s\n", audioBuffer->uri().c_str(), st.c_str());
	nextFun = &WaveLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void WaveLoader::commitData(TaskPool* taskPool)
{
	audioBuffer->addSubBuffer(0, pcmData.bytePtr(), pcmData.sizeInByte());
	audioBuffer->state = Resource::Loaded;
	roAssert(audioBuffer->loader == this);
	audioBuffer->loader = NULL;
	delete this;
}

void WaveLoader::abort(TaskPool* taskPool)
{
	audioBuffer->state = Resource::Aborted;
	roAssert(audioBuffer->loader == this);
	audioBuffer->loader = NULL;
	delete this;
}

Resource* resourceCreateWav(ResourceManager* mgr, const char* uri)
{
	return new AudioBuffer(uri);
}

bool resourceLoadWav(ResourceManager* mgr, Resource* resource)
{
	AudioBuffer* audioBuffer = dynamic_cast<AudioBuffer*>(resource);
	if(!audioBuffer)
		return false;

	TaskPool* taskPool = mgr->taskPool;
	WaveLoader* loaderTask = new WaveLoader(audioBuffer, mgr);
	audioBuffer->taskReady = audioBuffer->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingWav(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".wav"))
		return false;

	createFunc = resourceCreateWav;
	loadFunc = resourceLoadWav;
	return true;
}
