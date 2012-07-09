#include "pch.h"
#include "roTextResource.h"
#include "roByteOrder.h"
#include "roCpuProfiler.h"
#include "roFileSystem.h"
#include "roLog.h"
#include "roTypeCast.h"

namespace ro {

TextResource::TextResource(const char* uri)
	: Resource(uri)
{
}

struct TextLoader : public Task
{
	TextLoader(TextResource* t, ResourceManager* mgr)
		: stream(NULL), text(t), manager(mgr)
		, nextFun(&TextLoader::load)
	{}

	~TextLoader()
	{
		if(stream) fileSystem.closeFile(stream);
	}

	override void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void convert(TaskPool* taskPool);
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	TextResourcePtr text;
	ResourceManager* manager;

	String data;
	void (TextLoader::*nextFun)(TaskPool*);
};

void TextLoader::run(TaskPool* taskPool)
{
	if(text->state == Resource::Aborted || !taskPool->keepRun())
		nextFun = &TextLoader::abort;

	(this->*nextFun)(taskPool);
}

void TextLoader::load(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	Status st = Status::ok;

roEXCP_TRY
	if(text->state == Resource::Aborted) roEXCP_THROW;
	if(!stream) st = fileSystem.openFile(text->uri(), stream);
	if(!st) roEXCP_THROW;

	static const unsigned loopCount = 10;
	char buf[1024];

	// If data not ready, give up in this round and do it again in next schedule
	if(fileSystem.readWillBlock(stream, sizeof(buf) * loopCount))
		return reSchedule();

	for(unsigned i=0; i<loopCount; ++i) {
		roUint64 bytesRead = 0;
		st = fileSystem.read(stream, buf, sizeof(buf), bytesRead);

		if(st == Status::file_ended) {
			nextFun = &TextLoader::convert;
			break;
		}

		if(!st) roEXCP_THROW;
		data.append(buf, num_cast<size_t>(bytesRead));
	}

roEXCP_CATCH
	roLog("error", "TextLoader: Fail to load '%s', reason: %s\n", text->uri().c_str(), st.c_str());
	nextFun = &TextLoader::abort;
	return reSchedule(false, taskPool->mainThreadId());

roEXCP_END

	return reSchedule(false, ~taskPool->mainThreadId());
}

void TextLoader::convert(TaskPool* taskPool)
{
	unsigned char bomUtf8[] = { 0xEF, 0xBB, 0xBF };
	unsigned char bomUtf16BE[] = { 0xFE, 0xFF };
	unsigned char bomUtf16LE[] = { 0xFF, 0xFE };

	// UTF-8 BOM
	if(data.size() >= 3 && roStrnCmp(data.c_str(), (char*)bomUtf8, roCountof(bomUtf8)) == 0)
		data.erase(0, 3);

	// UTF-16 big endian
	else if(data.size() >= 2 && roStrnCmp(data.c_str(), (char*)bomUtf16BE, roCountof(bomUtf16BE)) == 0) {
		for(roSize i=0; i<data.size() - 2; i+=2)
			(roUtf16&)data[i] = roBEToHost((roUtf16&)data[i]);

		String tmp;
		tmp.fromUtf16((roUtf16*)&data[2], data.size() - 2);
		roSwap(data, tmp);
	}

	// UTF-16 little endian
	else if(data.size() >= 2 && roStrnCmp(data.c_str(), (char*)bomUtf16LE, roCountof(bomUtf16LE)) == 0) {
		for(roSize i=0; i<data.size() - 2; i+=2)
			(roUtf16&)data[i] = roLEToHost((roUtf16&)data[i]);

		String tmp;
		tmp.fromUtf16((roUtf16*)&data[2], data.size() - 2);
		roSwap(data, tmp);
	}

	nextFun = &TextLoader::commit;
	return reSchedule(false, taskPool->mainThreadId());
}

void TextLoader::commit(TaskPool* taskPool)
{
	roSwap(text->data, data);
	text->state = Resource::Loaded;

	delete this;
}

void TextLoader::abort(TaskPool* taskPool)
{
	text->state = Resource::Aborted;
	delete this;
}

Resource* resourceCreateText(ResourceManager* mgr, const char* uri)
{
	return new TextResource(uri);
}

bool resourceLoadText(ResourceManager* mgr, Resource* resource)
{
	TextResource* text = dynamic_cast<TextResource*>(resource);
	if(!text)
		return false;

	TaskPool* taskPool = mgr->taskPool;
	TextLoader* loaderTask = new TextLoader(text, mgr);
	text->taskReady = text->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingText(const char* uri, void*& createFunc, void*& loadFunc)
{
	static const char* extensions[] = {
		".txt", ".html", ".htm", ".xml", ".js", ".css"
	};

	for(roSize i=0; i<roCountof(extensions); ++i) {
		if(uriExtensionMatch(uri, extensions[i])) {
			createFunc = resourceCreateText;
			loadFunc = resourceLoadText;
		}
	}

	return false;
}

}	// namespace ro
