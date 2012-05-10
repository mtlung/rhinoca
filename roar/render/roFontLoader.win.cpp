#include "pch.h"
#include "roFont.h"
#include "roCanvas.h"
#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include "../roSubSystems.h"
#include "../base/roStopWatch.h"
#include <stdio.h>
#include <algorithm>

namespace ro {

namespace {

struct Glyph {
	roUint16 codePoint;					/// The unicode that identify the character
	roUint16 kerningIndex;				/// Index to the kerning table to begin the kerning pair search
	roUint16 kerningCount;				/// Number of kerning associated with this glyph
	roUint16 texIndex;					/// Index to the texture table
	roUint16 width, height;				/// Width and height for the glyph
	roUint16 texOffsetX, texOffsetY;	/// Defines the location in the texture
	roUint16 texSizeX, texSizeY;
	roInt16 originX, originY;
	roInt16 advanceX, advanceY;

	bool operator<(const Glyph& rhs) {
		return codePoint < rhs.codePoint;
	}
};

struct KerningPair {
	unsigned first, second;
	int offset;
};

struct FontData
{
	FontData();
	~FontData() { if(hFont) ::DeleteObject(hFont); }

	ConstString fontName;
	roUint32 fontHash;
	unsigned fontSize;
	unsigned fontWeight;
	bool italic;

	HFONT hFont;
	TEXTMETRIC tm;

	Array<TexturePtr> textures;
	Array<KerningPair> kerningPairs;	/// Sorted by KerningPair.first and then KerningPair.second
	Array<Glyph> glyphs;				/// Array of Glyph sorted by the code point for fast searching

	roUint16 currentTextureIdx;
	roUint16 dstX, dstY;
};

FontData::FontData()
	: fontSize(0)
	, fontWeight(FW_DONTCARE)
	, italic(false)
	, fontHash(0)
	, hFont(NULL)
	, currentTextureIdx(0)
	, dstX(0), dstY(0)
{
}

struct Request
{
	roUint32 fontHash;
	roUint16 codepoint;
};

struct Reply
{
	roUint32 fontHash;
	roUint16 texIndex;
	Array<Glyph> glyphs;
	Array<char> bitmapBuf;
};

struct FontImpl : public Font
{
	explicit FontImpl(const char* uri)
		: Font(uri)
		, hdc(NULL)
		, currnetFontForDraw(0)
	{
		hdc = ::CreateCompatibleDC(NULL);
	}

	~FontImpl()
	{
		if(hdc) ::DeleteDC(hdc);
	}

	override bool setStyle(const char* styleStr);

	override void draw(const char* utf8Str, float x, float y, float maxWidth, Canvas&);

	HDC hdc;	/// It is used when drawing text

	Array<FontData> typefaces;
	Array<Request> requestMainThread, requestLoadThread;
	Array<Reply> replys;

	roSize currnetFontForDraw;
};

struct Pred {
	static bool fontDataEqual(const FontData& f, const roUint32& h) {
		return f.fontHash == h;
	}
	static bool glyphLess(const Glyph& g, const roUint16& c) {
		return g.codePoint < c;
	}
	static bool glyphEqual(const Glyph& g, const roUint16& c) {
		return g.codePoint == c;
	}
};

}	// namespace

Resource* resourceCreateFont(const char* uri, ResourceManager* mgr)
{
	if(roStrCaseCmp("win.fnt", uri) == 0)
		return new FontImpl(uri);
	return NULL;
}

class FontLoader : public Task
{
public:
	FontLoader(FontImpl* f)
		: font(f)
		, hdc(NULL)
		, texDimension(512)
		, nextFun(&FontLoader::emumTypeface)
	{}

	~FontLoader()
	{
		if(hdc) ::DeleteDC(hdc);
	}

	override void run(TaskPool* taskPool);

protected:
	void checkRequest(TaskPool* taskPool);
	void processRequest(TaskPool* taskPool);
	void emumTypeface(TaskPool* taskPool);
	void requestAbort(TaskPool* taskPool);
	void cleanup(TaskPool* taskPool);

	FontImpl* font;

	HDC hdc;
	roUint16 texDimension;

	void (FontLoader::*nextFun)(TaskPool*);
};

void FontLoader::run(TaskPool* taskPool)
{
	if(font->state == Resource::Aborted && nextFun != &FontLoader::cleanup)
		nextFun = &FontLoader::requestAbort;

	(this->*nextFun)(taskPool);
}

void FontLoader::checkRequest(TaskPool* taskPool)
{
	roDetectFrameSpike("checkRequest");

	font->requestMainThread.swap(font->requestLoadThread);

	static int count = 0;
	// Process reply
	for(roSize i=0; i<font->replys.size(); ++i)
	{
		Reply& reply = font->replys[i];

		FontData* fontData = font->typefaces.find(reply.fontHash, Pred::fontDataEqual);
		roAssert(fontData);

	if(reply.glyphs.size() > count) {
		count = reply.glyphs.size();
		printf("\rcount=%d, total=%d\n", count, fontData->glyphs.size());
	}
		{roDetectFrameSpike("insert");
		// Insert the glyph from reply.glyphs into fontData->glyphs in asscending order of the code point
		fontData->glyphs.insert(fontData->glyphs.size(), reply.glyphs.begin(), reply.glyphs.end());
/*		for(roSize j=0; j<reply.glyphs.size(); ++j) {
			roAssert(reply.glyphs[j].texIndex == reply.texIndex);
			struct Pred { static bool less(const Glyph& lhs, const Glyph& rhs) {
				return lhs.codePoint < rhs.codePoint;
			}};
			fontData->glyphs.insertSorted(reply.glyphs[j], &Pred::less);
		}*/
//		roInsertionSort(fontData->glyphs.begin(), fontData->glyphs.end());
		std::sort(fontData->glyphs.begin(), fontData->glyphs.end());
		}

		// Create new texture if necessary
		if(reply.texIndex >= fontData->textures.size()) {
			TexturePtr texture = new Texture("");
			texture->handle = roRDriverCurrentContext->driver->newTexture();

			bool ok = roRDriverCurrentContext->driver->initTexture(
				texture->handle, texDimension, texDimension, 1, roRDriverTextureFormat_A, roRDriverTextureFlag_None);
			if(!ok) {
				fontData->textures.popBack();
				continue;
			}

			// Zero the pixels
			roUint8* texPtr = (roUint8*)roRDriverCurrentContext->driver->mapTexture(
				texture->handle, roRDriverMapUsage_Write, 0, 0);
			roZeroMemory(texPtr, texDimension * texDimension * 1);
			roRDriverCurrentContext->driver->unmapTexture(texture->handle, 0, 0);

			fontData->textures.pushBack(texture);
		}

		// Fill in the texture
		TexturePtr tex = fontData->textures[reply.texIndex];
		roUint8* texPtr = (roUint8*)roRDriverCurrentContext->driver->mapTexture(
			tex->handle, roRDriverMapUsage_ReadWrite, 0, 0);

		// "Adds" the loaded glyphs to the texture
		for(roSize j=0; j<reply.bitmapBuf.size(); ++j, ++texPtr)
			*texPtr += reply.bitmapBuf[j];

		roRDriverCurrentContext->driver->unmapTexture(tex->handle, 0, 0);
	}

	font->replys.clear();

	nextFun = &FontLoader::processRequest;
	return reSchedule(false, ~taskPool->mainThreadId());
}

static FontData* _findFontData(Array<FontData>& typefaces, roUint32 hash)
{
	for(roSize i=0; i<typefaces.size(); ++i)
	{
		FontData& fontData = typefaces[i];
		if(fontData.fontHash == hash)
			return &fontData;
	}

	return NULL;
}

static void _initWinFont(FontData& fontData, HDC hdc)
{
	Array<roUint16> wStr;
	{	// Convert the source uri to utf 16
		roSize len = 0;
		Status st = roUtf8ToUtf16(NULL, len, fontData.fontName.c_str()); if(!st) return;

		wStr.resize(len);
		st = roUtf8ToUtf16((roUint16*)&wStr[0], len, fontData.fontName.c_str()); if(!st) return;
		wStr.pushBack(0);
	}

	// http://msdn.microsoft.com/en-us/library/windows/desktop/dd183499%28v=vs.85%29.aspx
	fontData.hFont = ::CreateFontW(
		fontData.fontSize, 0, 0, 0, fontData.fontWeight,
		fontData.italic, FALSE, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
		wStr.castedPtr<wchar_t>()
	);

	::SelectObject(hdc, fontData.hFont);
	::GetTextMetricsW(hdc, &fontData.tm);

	// Get the kerning table
	DWORD pairCount = ::GetKerningPairsW(hdc, 0, NULL);
	Array<KERNINGPAIR> kerningPair(pairCount);
	::GetKerningPairsW(hdc, pairCount, kerningPair.typedPtr());

	fontData.kerningPairs.reserve(pairCount);
	for(roSize i=0; i<pairCount; ++i) {
		if(kerningPair[i].iKernAmount == 0)
			continue;

		// Make sure the kerning table is sorted for fast query
		KerningPair k = { kerningPair[i].wFirst, kerningPair[i].wSecond, kerningPair[i].iKernAmount };
		struct Pred { static bool less(const KerningPair& lhs, const KerningPair& rhs) {
			if(lhs.first == rhs.first) return lhs.second < rhs.second;
			return lhs.first < rhs.first;
		}};
		fontData.kerningPairs.insertSorted(k, &Pred::less);
	}
	fontData.kerningPairs.condense();
}

// Copy a texture from one memory to another memory
// TODO: Move to texture utilities
static void _texBlit(
	unsigned bytePerPixel,
	const char* srcPtr, unsigned srcX, unsigned srcY, unsigned srcWidth, unsigned srcHeight, unsigned srcRowBytes,
	      char* dstPtr, unsigned dstX, unsigned dstY, unsigned dstWidth, unsigned dstHeight, unsigned dstRowBytes)
{
	const char* pSrc = srcPtr;
	char* pDst = dstPtr;
	for(unsigned sy=srcY, dy=dstY; sy < srcY + srcHeight && dy < dstY + dstHeight; ++sy, ++dy) {
		pSrc = srcPtr + srcRowBytes * sy + srcX * bytePerPixel;
		pDst = dstPtr + dstRowBytes * dy + dstX * bytePerPixel;

		unsigned rowLen = roMinOf2(srcWidth - srcX, dstWidth - dstX);
		roMemcpy(pDst, pSrc, rowLen * bytePerPixel);
	}
}

void FontLoader::processRequest(TaskPool* taskPool)
{
	ro::Array<char> glyhpBitmapBuf;

	for(roSize i=0; i<font->requestLoadThread.size(); ++i)
	{
		Request& request = font->requestLoadThread[i];
		FontData* fontData = _findFontData(font->typefaces, request.fontHash);
		roAssert(fontData && "Hash value should already be registered by Font::setStyle()");

		// Init the win32 font if not yet
		if(!fontData->hFont)
			_initWinFont(*fontData, hdc);
		if(!fontData->hFont)
			continue;

		// Find out the reply object
		Reply* reply = NULL;
		for(roSize j=0; j<font->replys.size(); ++j) {
			if(request.fontHash == font->replys[j].fontHash)
				reply = &font->replys[j];
		}

		// If no reply object exist yet, create one
		if(!reply) {
			reply = &font->replys.pushBack();
			if(!reply)
				continue;

			reply->texIndex = fontData->currentTextureIdx;
			reply->fontHash = request.fontHash;
			reply->bitmapBuf.resize(texDimension * texDimension, 0);
		}

		// See if the codepoint already exist in the font, and the reply object
		roUint16 codePoint = request.codepoint;
		Glyph* g = roLowerBound(fontData->glyphs.typedPtr(), fontData->glyphs.size(), codePoint, &Pred::glyphLess);
		if(g && g->codePoint == codePoint)
			continue;

		if(reply->glyphs.find(codePoint, &Pred::glyphEqual))
			continue;

		g = &reply->glyphs.pushBack();
		g->codePoint = codePoint;
		g->kerningIndex = 0;
		g->kerningCount = 0;

		// Find the kerning for this code point
		if(!fontData->kerningPairs.isEmpty()) {
			struct Pred {
				static bool kerningLowerBound(const KerningPair& k, const roUint16& c) {
					return k.first < c;
				}
				static bool kerningUpperBound(const roUint16& c, const KerningPair& k) {
					return c < k.first;
				}
			};

			KerningPair* k1 = roLowerBound(fontData->kerningPairs.typedPtr(), fontData->kerningPairs.size(), codePoint, &Pred::kerningLowerBound);
			KerningPair* k2 = roUpperBound(fontData->kerningPairs.typedPtr(), fontData->kerningPairs.size(), codePoint, &Pred::kerningUpperBound);
			if(k1 && k1->first == codePoint) {
				roAssert(k2 > k1);
				g->kerningIndex = num_cast<roUint16>(k1 - fontData->kerningPairs.begin());
				g->kerningCount = num_cast<roUint16>(k2 - k1);
			}
		}

		// Get the glyph bitmaps
		// Reference: http://opensource.apple.com/source/WebCore/WebCore-1C25/platform/cairo/cairo/src/cairo-win32-font->c
		// http://www.winehq.org/pipermail/wine-patches/2002-July/002790.html
		GLYPHMETRICS glyphMetrics;
		static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
		::SelectObject(hdc, fontData->hFont);
		roSize bufSize = ::GetGlyphOutlineW(hdc, codePoint, GGO_GRAY8_BITMAP, &glyphMetrics, 0, NULL, &matrix);
		if(GDI_ERROR == bufSize) {
			roLog("warn", "GetGlyphOutlineW for code point %s failed\n", codePoint);
			continue;
		}

		// Get the glyph outline
		glyhpBitmapBuf.resize(bufSize);
		::GetGlyphOutlineW(hdc, codePoint, GGO_GRAY8_BITMAP, &glyphMetrics, bufSize, glyhpBitmapBuf.bytePtr(), &matrix);

		// Fail if the size of a glyph is too large
		if(glyphMetrics.gmBlackBoxX > texDimension || glyphMetrics.gmBlackBoxY > texDimension) {
			roLog("warn", "Glyph size of %d*%d was too large to fit into texture with size %d*%d\n",
				glyphMetrics.gmBlackBoxX, glyphMetrics.gmBlackBoxY, texDimension, texDimension);
			continue;
		}

		// Scale up from GGO_GRAY8_BITMAP to 0 - 255
		roUint64 pixelSum = 0;
		for(roSize i=0; i<glyhpBitmapBuf.size(); ++i) {
			int c = glyhpBitmapBuf[i];
			pixelSum += c;
			c = int(c * (float)255 / 64);
			glyhpBitmapBuf[i] = num_cast<char>(c);
		}

		// If the bitmap doesn't has any visual pixel, force the box to be zero
		if(pixelSum == 0)
			glyphMetrics.gmBlackBoxX = glyphMetrics.gmBlackBoxY = 0;

		// Increment the position in the destination texture
		// TODO: Add border
		if(fontData->dstX + glyphMetrics.gmBlackBoxX > texDimension) {
			fontData->dstX = 0;
			fontData->dstY = num_cast<roUint16>(fontData->dstY + fontData->tm.tmHeight);
		}

		// Condition for allocating new texture, for simplicity only one texture update at a time,
		// so if we detected a new texture is needed, we simply ignore the remaining request.
		bool textureFull = num_cast<roUint16>(fontData->dstY + fontData->tm.tmHeight) > texDimension;
		if(textureFull) {
			++fontData->currentTextureIdx;
			reply->glyphs.popBack();
			fontData->dstX = fontData->dstY = 0;
			nextFun = &FontLoader::checkRequest;
			return reSchedule(false, taskPool->mainThreadId());
		}

		// Copy the outline bitmap to the texture atlas
		roSize rowLen = roAlignCeiling(glyphMetrics.gmBlackBoxX, 4u);
		if(!glyhpBitmapBuf.isEmpty()) _texBlit(1,
			glyhpBitmapBuf.bytePtr(), 0, 0, glyphMetrics.gmBlackBoxX, glyphMetrics.gmBlackBoxY, rowLen,
			reply->bitmapBuf.bytePtr(), fontData->dstX, fontData->dstY, texDimension, texDimension, texDimension * 1);

		g->texIndex = fontData->currentTextureIdx;
		g->texOffsetX = fontData->dstX;
		g->texOffsetY = fontData->dstY;
		g->texSizeX = num_cast<roUint16>(glyphMetrics.gmBlackBoxX);
		g->texSizeY = num_cast<roUint16>(glyphMetrics.gmBlackBoxY);

		g->originX = num_cast<roInt16>(glyphMetrics.gmptGlyphOrigin.x);
		g->originY = num_cast<roInt16>(glyphMetrics.gmptGlyphOrigin.y);
		g->advanceX = num_cast<roInt16>(glyphMetrics.gmCellIncX);
		g->advanceY = num_cast<roInt16>(glyphMetrics.gmCellIncY);

		fontData->dstX += num_cast<roUint16>(glyphMetrics.gmBlackBoxX);
	}

	font->requestLoadThread.clear();

	nextFun = &FontLoader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

static int CALLBACK _enumFamCallBack(const LOGFONTW* lplf, const TEXTMETRICW* lpntm, DWORD fontType, LPARAM)
{
	String str;
	roVerify(str.fromUtf16((const roUtf16*)lplf->lfFaceName));
	roSubSystems->fontMgr->registerFontTypeface(str.c_str(), "win32Font");

	return TRUE;
}

// Reference: http://msdn.microsoft.com/en-us/library/dd162615%28v=vs.85%29.aspx
void FontLoader::emumTypeface(TaskPool* taskPool)
{
	if(!roSubSystems || !roSubSystems->fontMgr) {
		nextFun = &FontLoader::requestAbort;
		return reSchedule(false, taskPool->mainThreadId());
	}

	if(!hdc)
		hdc = ::CreateCompatibleDC(NULL);

	::EnumFontFamiliesExW(hdc, NULL, _enumFamCallBack, (LPARAM)this, 0);

	nextFun = &FontLoader::checkRequest;
	return reSchedule(false, taskPool->mainThreadId());
}

void FontLoader::requestAbort(TaskPool* taskPool)
{
	font->state = Resource::Aborted;
	nextFun = &FontLoader::cleanup;
	return reSchedule(false, taskPool->mainThreadId());
}

void FontLoader::cleanup(TaskPool* taskPool)
{
	if(font->state != Resource::Aborted)
		font->state = Resource::Loaded;

	delete this;
}

Resource* resourceCreateWin32Font(ResourceManager* mgr, const char* uri, StringHash type)
{
	if(type != stringHash("Font"))
		return NULL;
	return new FontImpl(uri);
}

bool resourceLoadWin32Font(ResourceManager* mgr, Resource* resource)
{
	FontImpl* font = dynamic_cast<FontImpl*>(resource);
	if(!font)
		return false;

	TaskPool* taskPool = mgr->taskPool;
	FontLoader* loaderTask = new FontLoader(font);
	font->taskReady = font->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool FontImpl::setStyle(const char* styleStr)
{
	roUint32 hash = stringHash(styleStr, 0);

	for(roSize i=0; i<typefaces.size(); ++i) {
		if(typefaces[i].fontHash == hash) {
			currnetFontForDraw = i;
			return true;
		}
	}

	FontData fontData;
	fontData.fontName = styleStr;
	fontData.fontSize = 30;
	fontData.fontWeight = FW_DONTCARE;
	fontData.italic = false;
	fontData.fontHash = hash;
	currnetFontForDraw = typefaces.size();
	typefaces.pushBack(fontData);

	return true;
}

void FontImpl::draw(const char* utf8Str, float x_, float y_, float maxWidth, Canvas& c)
{
	if(typefaces.isEmpty())
		return;

	FontData& fontData = typefaces[currnetFontForDraw];

	float x = x_, y = y_;

	// Pointer to the last and the current glyph
	Glyph* g1 = NULL, *g2 = NULL;

	roSize len = roStrLen(utf8Str);
	for(roUint16 w; len; g1 = g2)
	{
		int utf8Consumed = roUtf8ToUtf16Char(w, utf8Str, len);
		if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

		utf8Str += utf8Consumed;
		len -= utf8Consumed;

		// Search for the glyph by the given code point
		g2 = roLowerBound(fontData.glyphs.typedPtr(), fontData.glyphs.size(), w, &Pred::glyphLess);

		if(g2 && g2->codePoint == w) {
			// Calculate kerning
			if(g1 && !fontData.kerningPairs.isEmpty()) {
				struct Pred { static bool kerningLowerBound(const KerningPair& k, const roUint16& c) {
					return k.second < c;
				}};

				KerningPair* k = roLowerBound(&fontData.kerningPairs[g1->kerningIndex], g1->kerningCount, w, &Pred::kerningLowerBound);
				if(k && k->second == w)
					x += k->offset;
			}

			roRDriverTexture* tex = fontData.textures[g2->texIndex]->handle;
			float srcx = g2->texOffsetX,	srcy = g2->texOffsetY;
			float srcw = g2->texSizeX,		srch = g2->texSizeY;
			float dstx = x + g2->originX,	dsty = y - g2->originY;

			c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, srcw, srch);
			x += g2->advanceX;
			y += g2->advanceY;
		}
		else {
			Request req = { fontData.fontHash, w };
			requestMainThread.pushBack(req);
		}

		if(w == L'\n') {
			x = x_;
			y += fontData.tm.tmHeight;
		}
	}
}

}	// namespace ro
