#include "pch.h"
#include "truetypefont.h"
#include "../common.h"
#include "../rhlog.h"
#include "../../roar/base/roFileSystem.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u) rhinoca_malloc(x)
#define STBTT_free(x,u) rhinoca_free(x)
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

rhuint8* TrueTypeFont::bake(unsigned fontIdx, unsigned fontPixelHeight, int codepoint, int* width, int* height, int* xoff, int* yoff)
{
	if(!impl) return NULL;

	float scale = stbtt_ScaleForPixelHeight(&impl->fontInfo, (float)fontPixelHeight);
	return stbtt_GetCodepointBitmap(&impl->fontInfo, 0, scale, codepoint, width, height, xoff, yoff);
}

void TrueTypeFont::freeBitmap(rhuint8* bitmap)
{
	if(!impl) return;
	stbtt_FreeBitmap(bitmap, NULL);
}

void TrueTypeFont::getBoundingBox(unsigned fontPixelHeight, const int* codePointStr, unsigned* width, unsigned* height)
{
	unsigned w = 0;
	unsigned h = 0;
	unsigned widest = 0;

	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&impl->fontInfo, &ascent, &descent, &lineGap);
	unsigned lineHeight = ascent - descent;

	int lastCodePoint = 0;

	while(int c = *(codePointStr++))
	{
		if(c != '\n') {
			int advanceWidth, leftSideBearing;
			stbtt_GetCodepointHMetrics(&impl->fontInfo, c, &advanceWidth, &leftSideBearing);

			if(w == 0)	// Beginning of line
				w += leftSideBearing + advanceWidth;
			else
				w += advanceWidth + stbtt_GetCodepointKernAdvance(&impl->fontInfo, lastCodePoint, c);
		}
		else {	// Handling of new line
			widest = w > widest ? w : widest;
			w = 0;
			h += lineHeight + lineGap;
		}

		lastCodePoint = c;
	}

	h += lineHeight;

	const float scale = stbtt_ScaleForPixelHeight(&impl->fontInfo, (float)fontPixelHeight);

	w = (unsigned)(scale * w + 0.5f);
	h = (unsigned)(scale * h + 0.5f);

	if(width) *width = w;
	if(height) *height = h;
}

void TrueTypeFont::getMetrics(int* ascent, int* descent, int* lineGap)
{
	if(!impl) return;
}

}	// namespace Render

namespace Loader {

using namespace ro;
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
		if(stream) fileSystem.closeFile(stream);
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
	unsigned size = 0;
	const unsigned bufIncSize = 8 * 1024;
	unsigned bufSize = bufIncSize;
	rhbyte* buf = (rhbyte*)rhinoca_malloc(bufSize);
	rhbyte* p = buf;

	Status st;

	if(font->state == Resource::Aborted) goto Abort;
	if(!stream) st = fileSystem.openFile(font->uri(), stream);
	if(!st) {
		rhLog("error", "TrueTypeFontLoader: Fail to open file '%s', reason: %s\n", font->uri().c_str(), st.c_str());
		goto Abort;
	}

	// TODO: It would be much more efficient if we can know the file size in advance
/*	while(unsigned read = (unsigned)fileSystem.read(stream, p, bufIncSize)) {
		buf = (rhbyte*)rhinoca_realloc(buf, bufSize, bufSize + read);
		size += read;
		bufSize += read;
		p = buf + size;
	}*/

	if(size == 0 || !impl->create(buf, size)) goto Abort;

	readyToCommit = true;
	return;

Abort:
	readyToCommit = true;
	aborted = true;
}

void TrueTypeFontLoader::commit()
{
	if(!aborted) {
		font->state = Resource::Loaded;
		roAssert(!font->impl);
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

	font->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());
	font->taskLoaded = taskPool->addFinalized(loaderTask, 0, font->taskReady, taskPool->mainThreadId());
	taskPool->finishAdd(font->taskReady);

	return true;
}

}	// namespace Loader
