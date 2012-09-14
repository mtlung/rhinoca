#include "pch.h"
#include "roFont.h"
#include "roCanvas.h"
#include "../base/roArray.h"
#include "../base/roCpuProfiler.h"
#include "../base/roLog.h"
#include "../base/roParser.h"
#include "../base/roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include "../roSubSystems.h"
#include "../base/roStopWatch.h"
#include <stdio.h>

// Some resource of fonts on windows:
// http://www.catch22.net/tuts/editor11.asp

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

	bool operator<(const Glyph& rhs) const {
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
	int fontSize;
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
	: fontHash(0)
	, fontSize(0)
	, fontWeight(FW_DONTCARE)
	, italic(false)
	, hFont(NULL)
	, currentTextureIdx(0)
	, dstX(0), dstY(0)
{
	roZeroMemory(&tm, sizeof(tm));
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

	override roStatus measure(const roUtf8* str, roSize maxStrLen, float maxWidth, TextMetrics& metrics);

	override void draw(const roUtf8* str, roSize maxStrLen, float x, float y, float maxWidth, Canvas&);

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
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if((font->state == Resource::Aborted || !taskPool->keepRun()) && nextFun != &FontLoader::cleanup)
		nextFun = &FontLoader::requestAbort;

	(this->*nextFun)(taskPool);
}

void FontLoader::checkRequest(TaskPool* taskPool)
{
	font->requestMainThread.swap(font->requestLoadThread);

	// Process reply
	for(roSize i=0; i<font->replys.size(); ++i)
	{
		Reply& reply = font->replys[i];

		FontData* fontData = font->typefaces.find(reply.fontHash, Pred::fontDataEqual);
		roAssert(fontData);

		// Insert the glyph from reply.glyphs into fontData->glyphs in asscending order of the code point
		fontData->glyphs.insert(fontData->glyphs.size(), reply.glyphs.begin(), reply.glyphs.end());
		roQuickSort(fontData->glyphs.begin(), fontData->glyphs.end());

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
			roSize rowBytes = 0;
			roUint8* texPtr = (roUint8*)roRDriverCurrentContext->driver->mapTexture(
				texture->handle, roRDriverMapUsage_Write, 0, 0, rowBytes);
			roZeroMemory(texPtr, rowBytes * texDimension);
			roRDriverCurrentContext->driver->unmapTexture(texture->handle, 0, 0);

			fontData->textures.pushBack(texture);
		}

		// Fill in the texture
		TexturePtr tex = fontData->textures[reply.texIndex];
		roSize rowBytes = 0;
		roUint8* texPtr = (roUint8*)roRDriverCurrentContext->driver->mapTexture(
			tex->handle, roRDriverMapUsage_ReadWrite, 0, 0, rowBytes);

		// "Adds" the loaded glyphs to the texture
		for(roSize y=0; y<tex->height(); ++y) {
			for(roSize x=0; x<tex->width(); ++x)
				*(texPtr + x) += reply.bitmapBuf[y * tex->width() + x];
			texPtr += rowBytes;
		}

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
	::GetTextMetricsW(hdc, (LPTEXTMETRICW)&fontData.tm);

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
			roLog("warn", "GetGlyphOutlineW for code point %d failed\n", codePoint);
			continue;
		}

		// Get the glyph outline
		glyhpBitmapBuf.resize(bufSize);
		::GetGlyphOutlineW(hdc, codePoint, GGO_GRAY8_BITMAP, &glyphMetrics, num_cast<DWORD>(bufSize), glyhpBitmapBuf.bytePtr(), &matrix);

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
		unsigned rowLen = roAlignCeiling(glyphMetrics.gmBlackBoxX, 4u);
		if(!glyhpBitmapBuf.isEmpty()) roTextureBlit(1, glyphMetrics.gmBlackBoxX, glyphMetrics.gmBlackBoxY,
			glyhpBitmapBuf.bytePtr(), 0, 0, glyphMetrics.gmBlackBoxY, rowLen, false,
			reply->bitmapBuf.bytePtr(), fontData->dstX, fontData->dstY, texDimension, texDimension * 1, false);

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
	return reSchedule(true, taskPool->mainThreadId());
}

static int CALLBACK _enumFamCallBack(const LOGFONTW* lplf, const TEXTMETRICW* lpntm, DWORD fontType, LPARAM)
{
	String str;
	roVerify(str.fromUtf16((const roUtf16*)lplf->lfFaceName));

	if(roSubSystems->fontMgr)
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

using namespace Parsing;

namespace { struct _Pair {
	FontImpl* impl;
	FontData* fontData;
};}

static void fontParserCallback(ParserResult* result, Parser* parser)
{
	FontImpl* impl = reinterpret_cast<_Pair*>(parser->userdata)->impl;
	FontData* fontData = reinterpret_cast<_Pair*>(parser->userdata)->fontData;

	if(roStrCmp(result->type, "fontFamily") == 0) {
		if(*result->begin == '"')
			result->begin++;
		if(*(result->end - 1) == '"')
			result->end--;
		fontData->fontName = ConstString(result->begin, result->end - result->begin);
	}
	else if(roStrCmp(result->type, "fontSize") == 0) {
		int ptSize = 10; char c1, c2;
		if(sscanf(result->begin, "%d%c%c", &ptSize, &c1, &c2) == 3 && c1 == 'p' && c2 == 't')
			fontData->fontSize = -MulDiv(ptSize, ::GetDeviceCaps(impl->hdc, LOGPIXELSY), 72);
		result = result;
	}
	else if(roStrCmp(result->type, "fontWeight") == 0) {
		if(roStrnStr(result->begin, result->end - result->begin, "bold"))
			fontData->fontWeight = FW_BOLD;
	}
	else if(roStrCmp(result->type, "fontStyle") == 0) {
		if(roStrnStr(result->begin, result->end - result->begin, "italic"))
			fontData->italic = true;
	}
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
	fontData.fontHash = hash;
	currnetFontForDraw = typefaces.size();

	// Give the font some default values
	// For the formula to calculate the font height:
	// http://msdn.microsoft.com/en-us/library/dd183499%28v=vs.85%29.aspx
	fontData.fontSize = -MulDiv(12, ::GetDeviceCaps(hdc, LOGPIXELSY), 72);
	fontData.fontWeight = FW_DONTCARE;
	fontData.italic = false;
	fontData.fontName = "arial";

	// Parser the style string and set the font params
	_Pair pair = { this, &fontData };
	Parser parser(styleStr, styleStr + roStrLen(styleStr), fontParserCallback, &pair);
	
	if(!ro::Parsing::font(&parser).once())
		roLog("warn", "Invalid CSS font style string '%s'\n", styleStr);

	typefaces.pushBack(fontData);

	return true;
}

template<class T>
struct RecordMaxValue
{
	RecordMaxValue(const T& v) : val(v), max(v) {}
	RecordMaxValue& operator=(const T& v) { val = v; if(val > max) max = val; return *this; }
	operator T&() { return val; }
	operator const T&() const { return val; }

	template<class U>
	T& operator+=(const U& rhs) { val += rhs; if(val > max) max = val; return *this; }

	T val, max;
};

roStatus FontImpl::measure(const roUtf8* str, roSize maxStrLen, float maxWidth, TextMetrics& metrics)
{
	roStatus st;

	if(typefaces.isEmpty())
		return roStatus::not_initialized;

	FontData& fontData = typefaces[currnetFontForDraw];

	float x_ = 0, y_ = 0;
	RecordMaxValue<float> x = x_, y = y_;
	y.max = (float)fontData.tm.tmHeight;

	// Pointer to the last and the current glyph
	Glyph* g1 = NULL, *g2 = NULL;

	bool someGlyphNotLoaded = false;

	roSize len = roMinOf2(roStrLen(str), maxStrLen);
	for(roUint16 w; len; g1 = g2)
	{
		int utf8Consumed = roUtf8ToUtf16Char(w, str, len);
		if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

		str += utf8Consumed;
		len -= utf8Consumed;
		g2 = NULL;

		// Handling of special characters
		if(w == L'\r') {
			x = x_;
			continue;
		}
		if(w == L'\n') {
			x = x_;
			y += fontData.tm.tmHeight;

			continue;
		}

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

			x += g2->advanceX;
			y += g2->advanceY;
		}
		else {
			someGlyphNotLoaded = true;
			Request req = { fontData.fontHash, w };
			requestMainThread.pushBack(req);
		}
	}

	metrics.width = x.max;
	metrics.height = y.max;
	metrics.lineSpacing = float(fontData.tm.tmHeight + fontData.tm.tmInternalLeading + fontData.tm.tmExternalLeading);

	if(someGlyphNotLoaded)
		return roStatus::not_loaded;

	return roStatus::ok;
}

struct GlyphCache
{
	float x, y;
	Glyph* glyph;
	roRDriverTexture* tex;
};

static void _drawLineOfText(FontData& fontData, const GlyphCache* cache, roSize count, float boundingWidth, Canvas& canvas)
{
	float offsetX = 0;
	float offsetY = 0;

	StringHash alignment = stringLowerCaseHash(canvas.textAlign());
	StringHash baseline = stringLowerCaseHash(canvas.textBaseline());

	if(alignment == stringHash("center"))
		offsetX -= boundingWidth / 2;
	else if(alignment == stringHash("start"))
		offsetX += 0;
	else if(alignment == stringHash("left"))
		offsetX += 0;
	else if(alignment == stringHash("end"))
		offsetX -= boundingWidth;
	else if(alignment == stringHash("right"))
		offsetX -= boundingWidth;

	// Reference: http://flylib.com/books/en/3.217.1.179/1/
	if(baseline == stringHash("top"))
		offsetY += fontData.tm.tmAscent;
	else if(baseline == stringHash("bottom"))
		offsetY -= fontData.tm.tmDescent;
	else if(baseline == stringHash("middle"))
		offsetY += (fontData.tm.tmAscent - fontData.tm.tmInternalLeading) / 2;

	for(roSize i=0; i<count; ++i) {
		const GlyphCache& c = cache[i];
		const Glyph& g = *cache[i].glyph;

		float x = (float)int(c.x + offsetX);	// NOTE: We ensure x and y are in integer
		float y = (float)int(c.y + offsetY);

		// TODO: Clip against current clip rect with transform.inverse
//		if(x + g.texSizeX < 0)
//			continue;
//		if(x >= canvas.width())
//			break;

		canvas.drawImage(
			c.tex, g.texOffsetX, g.texOffsetY, g.texSizeX, g.texSizeY,
			x, y, g.texSizeX, g.texSizeY
		);
	}
}

void FontImpl::draw(const roUtf8* str, roSize maxStrLen, float x_, float y_, float maxWidth, Canvas& canvas)
{
	if(typefaces.isEmpty())
		return;

	TinyArray<GlyphCache, 256> caches;

	FontData& fontData = typefaces[currnetFontForDraw];

	float x = x_, y = y_;

	// Pointer to the last and the current glyph
	Glyph* g1 = NULL, *g2 = NULL;

	canvas.beginDrawImageBatch();

	roSize len = roMinOf2(roStrLen(str), maxStrLen);
	for(roUint16 w; len; g1 = g2)
	{
		int utf8Consumed = roUtf8ToUtf16Char(w, str, len);
		if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

		str += utf8Consumed;
		len -= utf8Consumed;
		g2 = NULL;

		// Handling of special characters
		if(w == L'\r') {
			x = x_;
			continue;
		}
		if(w == L'\n') {
			// Text getting out of our canvas, no need to continue
			// TODO: Clip against current clip rect with transform.inverse
//			if(y > canvas.height())
//				break;

			_drawLineOfText(fontData, caches.begin(), caches.size(), x - x_, canvas);
			caches.clear();

			x = x_;
			y += fontData.tm.tmHeight;
			continue;
		}

		// Search for the glyph by the given code point
		g2 = roLowerBound(fontData.glyphs.typedPtr(), fontData.glyphs.size(), w, &Pred::glyphLess);

		if(g2 && g2->codePoint == w) {
			// Calculate kerning
			if(g1 && fontData.kerningPairs.isInRange(g1->kerningIndex)) {
				struct Pred { static bool kerningLowerBound(const KerningPair& k, const roUint16& c) {
					return k.second < c;
				}};

				KerningPair* k = roLowerBound(&fontData.kerningPairs[g1->kerningIndex], g1->kerningCount, w, &Pred::kerningLowerBound);
				if(k && k->second == w)
					x += k->offset;
			}

			GlyphCache cache = {
				x + g2->originX, y - g2->originY, g2,
				fontData.textures[g2->texIndex]->handle
			};
			caches.pushBack(cache);

			x += g2->advanceX;
			y += g2->advanceY;
		}
		else {
			Request req = { fontData.fontHash, w };
			requestMainThread.pushBack(req);
		}
	}

	// Flush the remaining glyph caches
	_drawLineOfText(fontData, caches.begin(), caches.size(), x - x_, canvas);
	caches.clear();

	canvas.endDrawImageBatch();

	if(!requestMainThread.isEmpty())
		roSubSystems->taskPool->resume(taskLoaded);
}

}	// namespace ro
