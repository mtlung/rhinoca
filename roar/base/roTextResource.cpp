#include "pch.h"
#include "roTextResource.h"
#include "roFileSystem.h"
#include "roLog.h"
#include "roTypeCast.h"

namespace ro {

TextResource::TextResource(const char* uri)
	: Resource(uri)
	, dataWithoutBOM(NULL)
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
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	TextResource* text;
	ResourceManager* manager;

	String data;
	void (TextLoader::*nextFun)(TaskPool*);
};

void TextLoader::run(TaskPool* taskPool)
{
	if(text->state == Resource::Aborted)
		nextFun = &TextLoader::abort;

	(this->*nextFun)(taskPool);
}

static const char* _removeBom(const char* str, unsigned& len)
{
	if(len < 3) return str;

	if( (str[0] == 0xFE && str[1] == 0xFF) ||
		(str[0] == 0xFF && str[1] == 0xFE))
	{
		roLog("error", "'%s' is encoded using UTF-16 which is not supported\n");
		len = 0;
		return NULL;
	}

	if(str[0] == 0xEF && str[1] == 0xBB && str[2] == 0xBF) {
		len -= 3;
		return str + 3;
	}
	return str;
}

void TextLoader::commit(TaskPool* taskPool)
{
	roSwap(text->data, data);
	text->state = Resource::Loaded;

	// Remove any BOM
	unsigned len;
	text->dataWithoutBOM = _removeBom(text->data.c_str(), len);

	delete this;
}

void TextLoader::load(TaskPool* taskPool)
{
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
			nextFun = &TextLoader::commit;
			break;
		}

		if(!st) roEXCP_THROW;
		data.append(buf, num_cast<size_t>(bytesRead));
	}

roEXCP_CATCH
	roLog("error", "TextLoader: Fail to load '%s', reason: %s\n", text->uri().c_str(), st.c_str());
	nextFun = &TextLoader::abort;

roEXCP_END

	return reSchedule(false, taskPool->mainThreadId());
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
