#include "pch.h"
#include "truetypefont.h"
#include "../vector.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace Render {

class TrueTypeFont::Impl
{
public:
	~Impl();

	bool create(rhbyte* data, unsigned length);

	rhbyte* fontFile;	/// Memory to the raw ttf file
	stbtt_fontinfo fontInfo;
};

TrueTypeFont::Impl::~Impl()
{
	rhinoca_free(fontFile);
}

bool TrueTypeFont::Impl::create(rhbyte* data, unsigned length)
{
	fontFile = data;
	return stbtt_InitFont(&fontInfo, data, 0);
}

TrueTypeFont::TrueTypeFont(const char* uri)
	: Font(uri), impl(NULL)
{
}

TrueTypeFont::~TrueTypeFont()
{
	delete impl;
}

bool TrueTypeFont::bake(unsigned fontIdx, unsigned fontSize)
{
	if(!impl) return false;
	return true;
}

void TrueTypeFont::getMetrics(int* ascent, int* descent, int* lineGap)
{
	if(!impl) return;
}

}	// namespace Render

namespace Loader {

using namespace Render;

class TrueTypeFontLoader : public Task
{
public:
	TrueTypeFontLoader(TrueTypeFont* f, ResourceManager* mgr)
		: stream(NULL), font(f), manager(mgr)
		, aborted(false), readyToCommit(false)
	{
		impl = new TrueTypeFont::Impl;
	}

	~TrueTypeFontLoader()
	{
		if(stream) io_close(stream, TaskPool::threadId());
		delete impl;
	}

	override void run(TaskPool* taskPool);

	void load();
	void commit();

	void* stream;
	TrueTypeFont* font;
	TrueTypeFont::Impl* impl;
	ResourceManager* manager;

	bool aborted;
	bool readyToCommit;
};	// TrueTypeFontLoader

void TrueTypeFontLoader::run(TaskPool* taskPool)
{
	if(!readyToCommit)
		load();
	else
		commit();
}

void TrueTypeFontLoader::load()
{
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	unsigned size = 0;
	const unsigned bufIncSize = 8 * 1024;
	unsigned bufSize = bufIncSize;
	rhbyte* buf = (rhbyte*)rhinoca_malloc(bufSize);
	rhbyte* p = buf;

	if(!stream) stream = io_open(rh, font->uri(), tId);
	if(!stream) {
		print(rh, "TrueTypeFontLoader: Fail to open file '%s'\n", font->uri().c_str());
		goto Abort;
	}

	while(unsigned read = (unsigned)io_read(stream, p, bufIncSize, tId)) {
		buf = (rhbyte*)rhinoca_realloc(buf, bufSize, bufSize + read);
		size += read;
		bufSize += read;
		p = buf + size;
	}

	if(size == 0 || !impl->create(buf, size)) goto Abort;

	readyToCommit = true;
	return;

Abort:
	readyToCommit = true;
	aborted = true;
}

void TrueTypeFontLoader::commit()
{
	int tId = TaskPool::threadId();

	if(!aborted) {
		font->state = Resource::Loaded;
		ASSERT(!font->impl);
		font->impl = impl;
		impl = NULL;
	}
	else
		font->state = Resource::Aborted;

	delete this;
}

Resource* createTrueTypeFont(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".ttf"))
		return new Render::TrueTypeFont(uri);
	return NULL;
}

bool loadTrueTypeFont(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".ttf")) return false;

	TaskPool* taskPool = mgr->taskPool;

	TrueTypeFont* font = dynamic_cast<TrueTypeFont*>(resource);

	TrueTypeFontLoader* loaderTask = new TrueTypeFontLoader(font, mgr);

	font->taskReady = taskPool->beginAdd(loaderTask, ~taskPool->mainThreadId());
	font->taskLoaded = taskPool->addFinalized(loaderTask, 0, font->taskReady, taskPool->mainThreadId());
	taskPool->finishAdd(font->taskReady);

	return true;
}

}	// namespace Loader
