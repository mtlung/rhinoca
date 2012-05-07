#include "pch.h"
#include "roFont.h"
#include "roCanvas.h"
#include "roRenderDriver.h"
#include "../base/roAlgorithm.h"
#include "../base/roArray.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roTypeCast.h"
#include "../math/roVector.h"
#include "../platform/roPlatformHeaders.h"
#include "../roSubSystems.h"

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
	static bool fontDataEqual(const FontData& f, const roUint32& k) {
		return f.fontHash == k;
	}
	static bool glyphLess(const Glyph& g, const roUint16& k) {
		return g.codePoint < k;
	}
	static bool glyphEqual(const Glyph& g, const roUint16& k) {
		return g.codePoint == k;
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
	font->requestMainThread.swap(font->requestLoadThread);

	// Process reply
	for(roSize i=0; i<font->replys.size(); ++i)
	{
		Reply& reply = font->replys[i];

		FontData* fontData = font->typefaces.find(reply.fontHash, Pred::fontDataEqual);
		roAssert(fontData);

		// Insert the glyph from reply.glyphs into fontData->glyphs in asscending order of the code point
		for(roSize j=0; j<reply.glyphs.size(); ++j) {
			roAssert(reply.glyphs[j].texIndex == reply.texIndex);

			Glyph* g = roLowerBound(fontData->glyphs.typedPtr(), fontData->glyphs.size(), reply.glyphs[j].codePoint, &Pred::glyphLess);
			roSize insertPos = g ? g - fontData->glyphs.typedPtr() : fontData->glyphs.size();

			fontData->glyphs.insert(insertPos, reply.glyphs[j]);
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
		FALSE, FALSE, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
		wStr.castedPtr<wchar_t>()
	);

	::SelectObject(hdc, fontData.hFont);
	::GetTextMetricsW(hdc, &fontData.tm);
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

	// TODO: Sort the request queue first for better performance
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
		for(roSize i=0; i<glyhpBitmapBuf.size(); ++i) {
			int c = glyhpBitmapBuf[i];
			c = int(c * (float)255 / 64);
			glyhpBitmapBuf[i] = num_cast<char>(c);
		}

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
	roVerify(str.fromUtf16((const roUint16*)lplf->lfFaceName));
	roSubSystems->fontMgr->registerFontTypeface(str.c_str(), "win.fnt");

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

	::EnumFontFamiliesW(hdc, NULL, _enumFamCallBack, (LPARAM)this);

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

bool resourceLoadWinfnt(Resource* resource, ResourceManager* mgr)
{
	if(roStrCaseCmp("win.fnt", resource->uri()) != 0) return false;

	TaskPool* taskPool = mgr->taskPool;

	FontImpl* font = dynamic_cast<FontImpl*>(resource);

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
	fontData.fontHash = hash;
	currnetFontForDraw = typefaces.size();
	typefaces.pushBack(fontData);

	return true;
}

void FontImpl::draw(const char* utf8Str, float x_, float y_, float maxWidth, Canvas& c)
{
	// TODO: Use windows's function to calculate text position
	// http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/28c492af-1bed-4415-9385-9b0345a41871
//	if(state != Loaded || textures.isEmpty())
//		return;

	roSize len = roStrLen(utf8Str);
	FontData& fontData = typefaces[currnetFontForDraw];

	float x = x_, y = y_;

/*	{	INT iDxArr[512];
		GCP_RESULTS cres;
		cres.lStructSize = sizeof(GCP_RESULTS);
		cres.lpOutString	= NULL;
		cres.lpOrder		= NULL;
		cres.lpDx		= iDxArr;
		cres.lpCaretPos		= NULL;
		cres.lpClass		= NULL;
		cres.lpGlyphs		= NULL;
		cres.nGlyphs		= 8;
		cres.nMaxFit		= 0;

		::SelectObject(hdc, fontData.hFont);
		DWORD dwRet = ::GetCharacterPlacement(hdc, L"AT Hello", 8, 0, &cres, GCP_USEKERNING);
		roAssert(dwRet != 0);
	}*/

	for(roUint16 w, prev = 0; len; prev = w)
	{
		int utf8Consumed = roUtf8ToUtf16Char(w, utf8Str, len);
		if(utf8Consumed < 1) break;	// Invalid utf8 string encountered

		utf8Str += utf8Consumed;
		len -= utf8Consumed;

		// Search for the glyph by the given code point
		struct Pred { static bool less(const Glyph& g, const roUint16& k) {
			return g.codePoint < k;
		}};
		Glyph* g = roLowerBound(fontData.glyphs.typedPtr(), fontData.glyphs.size(), w, &Pred::less);

		if(g && g->codePoint == w) {
			// Calculate kerning
/*			for(roSize i=g->kerningIndex; i<g->kerningIndex + g->kerningCount; ++i) {
				roAssert(kerningPairs[i].second == w);
				if(kerningPairs[i].first == prev)
					x += kerningPairs[i].offset;
			}*/

			roRDriverTexture* tex = fontData.textures[g->texIndex]->handle;
			float srcx = g->texOffsetX,		srcy = g->texOffsetY;
			float srcw = g->texSizeX,		srch = g->texSizeY;
			float dstx = x + g->originX,	dsty = y - g->originY;

			c.drawImage(tex, srcx, srcy, srcw, srch, dstx, dsty, srcw, srch);
			x += g->advanceX;
			y += g->advanceY;
		}
		else {
			Request req = { fontData.fontHash, w };
			requestMainThread.pushBack(req);
		}

		if(w == L'\n') {
			x = x_;
			if(g) y += g->height;	// TODO: Replace g->height with height for that font
		}
	}
}

}	// namespace ro
