#include "pch.h"
#include "textresource.h"
#include "platform.h"

TextResource::TextResource(const char* uri)
	: Resource(uri)
	, dataWithoutBOM(NULL)
{
}

namespace Loader {

Resource* createText(const char* uri, ResourceManager* mgr)
{
	if( uriExtensionMatch(uri, ".js") ||
		uriExtensionMatch(uri, ".css")
	)
		return new TextResource(uri);
	return NULL;
}

// 
class TextLoader : public Task
{
public:
	TextLoader(TextResource* t, ResourceManager* mgr)
		: stream(NULL), text(t), manager(mgr)
		, aborted(false), readyToCommit(false)
	{}

	~TextLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
	}

	override void run(TaskPool* taskPool);

protected:
	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	void* stream;
	TextResource* text;
	ResourceManager* manager;

	String data;
	bool aborted;
	bool readyToCommit;
};

void TextLoader::run(TaskPool* taskPool)
{
	if(!readyToCommit)
		load(taskPool);
	else
		commit(taskPool);
}

const char* removeBom(Rhinoca* rh, const char* str, unsigned& len)
{
	if(len < 3) return str;

	if( (str[0] == (char)0xFE && str[1] == (char)0xFF) ||
		(str[0] == (char)0xFF && str[1] == (char)0xFE))
	{
		print(rh, "'%s' is encoded using UTF-16 which is not supported\n");
		len = 0;
		return NULL;
	}

	if(str[0] == (char)0xEF && str[1] == (char)0xBB && str[2] == (char)0xBF) {
		len -= 3;
		return str + 3;
	}
	return str;
}

void TextLoader::commit(TaskPool* taskPool)
{
	int tId = TaskPool::threadId();

	if(!aborted) {
		text->data.swap(data);
		text->state = Resource::Loaded;

		// Remove any BOM
		unsigned len;
		text->dataWithoutBOM = removeBom( manager->rhinoca, text->data.c_str(), len);
	}
	else
		text->state = Resource::Aborted;

	delete this;
}

void TextLoader::load(TaskPool* taskPool)
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = io_open(rh, text->uri(), tId);
	if(!stream) {
		print(rh, "TextLoader: Fail to open file '%s'\n", text->uri().c_str());
		goto Abort;
	}

	static const unsigned loopCount = 10;
	char buf[1024];

	// If data not ready, give up in this round and do it again in next schedule
	if(!io_ready(stream, sizeof(buf) * loopCount, tId))
		return reSchedule();

	for(unsigned i=0; i<loopCount; ++i) {
		rhuint64 readCount = io_read(stream, buf, sizeof(buf), tId);

		if(readCount > 0) {
			data.append(buf, (size_t)readCount);
			continue;
		}

		readyToCommit = true;
		return;
	}

	return reSchedule();

Abort:
	aborted = true;
}

bool loadText(Resource* resource, ResourceManager* mgr)
{
	if( !uriExtensionMatch(resource->uri(), ".js") &&
		!uriExtensionMatch(resource->uri(), ".css")
	)
		return false;

	TaskPool* taskPool = mgr->taskPool;

	TextResource* text = dynamic_cast<TextResource*>(resource);

	TextLoader* loaderTask = new TextLoader(text, mgr);

	TaskId taskLoad = taskPool->beginAdd(loaderTask, ~taskPool->mainThreadId());
	text->taskReady = text->taskLoaded = taskPool->addFinalized(loaderTask, 0, taskLoad, taskPool->mainThreadId());
	taskPool->finishAdd(taskLoad);

	return true;
}

}	// namespace Loader
