#include "pch.h"
#include "textrenderer.h"
#include "driver.h"
#include "../map.h"
#include "../../roar/base/roStringHash.h"
#include "../../roar/base/roStringUtility.h"

using namespace ro;

namespace Render {

class TextLabelCache : public MapBase<rhuint32>::Node<TextLabelCache>
{
public:
	typedef MapBase<rhuint32>::Node<TextLabelCache> Super;
	TextLabelCache(rhuint32 key) : Super(key), refCount(0) {}

	unsigned refCount;
};	// TextLabelCache

class TextRenderer::Impl
{
public:
	Map<TextLabelCache> cache;
};

TextRenderer::TextRenderer()
	: impl(new Impl)
{
}

TextRenderer::~TextRenderer()
{
	delete impl;
}

/*GlyphCache::Glyph* TextRenderer::prepareGlyph(Font* font, unsigned fontIdx, unsigned fontPixelHeight, int codePoint)
{
	// Search for texture in glyph cache
	GlyphCache& cache = font->glyphCache;
	GlyphCache::Glyph* g = NULL;

	if(codePoint < 256) {
		g = &cache.ascii[codePoint];
		g = g->texture ? g : NULL;
	}
	else
		g = cache.glyphs.find(codePoint);

	// Cache miss, create the glyph now
	if(!g) {
		rhuint8* bitmap = font->bake(fontIdx, fontPixelHeight, codePoint, &g->width, &g->height, &g->xoff, &g->yoff);
		g->texture = Driver::createTexture(NULL, g->width, g->height, Driver::LUMINANCE, bitmap, Driver::LUMINANCE);
		font->freeBitmap(bitmap);
	}

	return g;
}*/

static rhuint32 appendHash(rhuint32 h, rhuint32 val) {
	return h * 65599 + val;
}

rhuint32 TextRenderer::makeHash(Font* font, unsigned fontPixelHeight, const char* utf8Str, unsigned utf8Len)
{
	RHASSERT(font && utf8Str);

	// Determine the string length
	unsigned strLen = 0;
	if(!roUtf8ToUtf32(NULL, strLen, utf8Str, utf8Len == 0 ? unsigned(-1) : utf8Len))
		return 0;

	rhuint32* codePointStr = (rhuint32*)rhinoca_malloc(sizeof(int) * strLen);
	if(!roUtf8ToUtf32(codePointStr, strLen, utf8Str, utf8Len)) {
		rhinoca_free(codePointStr);
		return 0;
	}

	StringHash hash = stringHash(utf8Str, utf8Len);
	hash = appendHash(hash, font->uri().hash());
	hash = appendHash(hash, fontPixelHeight);

	// Try to locate any cached value
	TextLabelCache* cache = impl->cache.find(hash);
	if(cache) {
		// TOOD: Check if the existing cache's property match those requested
		return hash;
	}

	// Else create the text label now
	cache = new TextLabelCache(hash);
	impl->cache.insertUnique(*cache);

//	unsigned width, height;
//	font->getBoundingBox(fontPixelHeight, codePointStr, &width, &height);

	while(*codePointStr != 0) {
	}

	rhinoca_free(codePointStr);

	return hash;
}

bool TextRenderer::drawText(rhuint32 hash, float x, float y, float* colorFloat4)
{
	TextLabelCache* cache = impl->cache.find(hash);
	if(!cache)
		return false;
	return true;
}

rhuint32 TextRenderer::drawText(Font* font, unsigned fontPixelHeight, const char* utf8Str, unsigned strLen, float x, float y, float* colorFloat4)
{
	rhuint32 h = makeHash(font, fontPixelHeight, utf8Str, strLen);
	RHVERIFY(drawText(h, x, y, colorFloat4));
	return h;
}

void TextRenderer::addReference(rhuint32 hash)
{
}

void TextRenderer::releaseReference(rhuint32 hash)
{
}

}	// namespace Render
