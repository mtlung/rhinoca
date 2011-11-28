#include "pch.h"
#include "textresource.h"
#include "common.h"
#include "rhlog.h"
#include "platform.h"

using namespace ro;

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
		if(stream) rhFileSystem.closeFile(stream);
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

const char* removeBom(const char* str, unsigned& len)
{
	if(len < 3) return str;

	if( (str[0] == (char)0xFE && str[1] == (char)0xFF) ||
		(str[0] == (char)0xFF && str[1] == (char)0xFE))
	{
		rhLog("error", "'%s' is encoded using UTF-16 which is not supported\n");
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
	if(!aborted) {
		roSwap(text->data, data);
		text->state = Resource::Loaded;

		// Remove any BOM
		unsigned len;
		text->dataWithoutBOM = removeBom(text->data.c_str(), len);
	}
	else
		text->state = Resource::Aborted;

	delete this;
}

void TextLoader::load(TaskPool* taskPool)
{
	Rhinoca* rh = manager->rhinoca;

	if(!stream) stream = rhFileSystem.openFile(rh, text->uri());
	if(!stream) {
		rhLog("error", "TextLoader: Fail to open file '%s'\n", text->uri().c_str());
		goto Abort;
	}

	static const unsigned loopCount = 10;
	char buf[1024];

	// If data not ready, give up in this round and do it again in next schedule
	if(!rhFileSystem.readReady(stream, sizeof(buf) * loopCount))
		return reSchedule();

	for(unsigned i=0; i<loopCount; ++i) {
		rhuint64 readCount = rhFileSystem.read(stream, buf, sizeof(buf));

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

	TaskId taskLoad = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());
	text->taskReady = text->taskLoaded = taskPool->addFinalized(loaderTask, 0, taskLoad, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
