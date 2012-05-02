#include "pch.h"
#include "roFont.h"
#include "roCanvas.h"
#include "roTexture.h"
#include "roRenderDriver.h"
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
	unsigned codePoint;			/// The unicode that identify the character
	roUint16 kerningIndex;		/// Index to the kerning table to begin the kerning pair search
	roUint16 kerningCount;		/// Number of kerning associated with this glyph
	roUint16 texIndex;			/// Index to the texture table
	roUint16 width, height;		/// Width and height for the glyph
	Vec2 texOffset, texSize;	/// Defines the location in the texture
};

struct KerningPair {
	unsigned first, second;
	int offset;
};

struct FontImpl : public Font
{
	explicit FontImpl(const char* uri)
		: Font(uri)
	{
		canvas.init();
	}

	override bool setStyle(const char* styleStr);

	override void draw(const char* utf8Str, float x, float y, float maxWidth, Canvas&);

// Attributes
	Array<TexturePtr> textures;
	Array<KerningPair> kerningPairs;	/// Sorted by KerningPair.first and then KerningPair.second
	Array<Glyph> glyphs;				/// Array of Glyph sorted by the code point for fast searching

	Canvas canvas;	/// Canvas for putting the font textures into the render target
};

}	// namespace

enum FontLoadingState {
	FontLoadingState_EmumTypeface,
	FontLoadingState_LoadMetric,
	FontLoadingState_LoadBitmaps,
	FontLoadingState_AllocateTexture,
	FontLoadingState_Cleanup,
	FontLoadingState_Commit,
	FontLoadingState_Finish,
	FontLoadingState_RequestAbort,
	FontLoadingState_Abort
};

Resource* resourceCreateFont(const char* uri, ResourceManager* mgr)
{
	if(roStrCaseCmp("win.fnt", uri) == 0)
		return new FontImpl(uri);
	return NULL;
}

class FontLoader : public Task
{
public:
	FontLoader(FontImpl* f, ResourceManager* mgr)
		: font(f), manager(mgr)
		, fontLoadingState(FontLoadingState_EmumTypeface)
		, hdc(NULL), hFont(NULL)
		, texDimension(1)
		, currentGlyphIdx(0), currentTextureIdx(0)
		, dstX(0), dstY(0)
		, mappedTexPtr(NULL)
	{}

	~FontLoader()
	{
		if(hFont) ::DeleteObject(hFont);
		if(hdc) ::DeleteDC(hdc);
	}

	override void run(TaskPool* taskPool);

protected:
	void emumTypeface(TaskPool* taskPool);
	void loadMetric(TaskPool* taskPool);
	void loadBitmaps(TaskPool* taskPool);
	void allocateTexture(TaskPool* taskPool);
	void requestAbort(TaskPool* taskPool);
	void cleanup(TaskPool* taskPool);

	FontImpl* font;
	ResourceManager* manager;
	FontLoadingState fontLoadingState;

	HDC hdc;
	HFONT hFont;
	TEXTMETRIC tm;

	// Variable used for reading font bitmaps
	unsigned texDimension;
	roSize currentGlyphIdx;
	roSize currentTextureIdx;
	unsigned dstX, dstY;
	char* mappedTexPtr;
};

void FontLoader::run(TaskPool* taskPool)
{
	if(font->state == Resource::Aborted && fontLoadingState != FontLoadingState_Cleanup)
		fontLoadingState = FontLoadingState_RequestAbort;

	if(fontLoadingState == FontLoadingState_EmumTypeface)
		emumTypeface(taskPool);
	else if(fontLoadingState == FontLoadingState_LoadMetric)
		loadMetric(taskPool);
	else if(fontLoadingState == FontLoadingState_LoadBitmaps)
		loadBitmaps(taskPool);
	else if(fontLoadingState == FontLoadingState_AllocateTexture)
		allocateTexture(taskPool);
	else if(fontLoadingState == FontLoadingState_RequestAbort)
		requestAbort(taskPool);
	else if(fontLoadingState == FontLoadingState_Cleanup)
		cleanup(taskPool);

	if(fontLoadingState == FontLoadingState_Finish) {
		font->state = Resource::Loaded;
		delete this;
		return;
	}

	if(fontLoadingState == FontLoadingState_Abort) {
		font->state = Resource::Aborted;
		delete this;
		return;
	}
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
		fontLoadingState = FontLoadingState_RequestAbort;
		return;
	}

	if(!hdc)
		hdc = ::CreateCompatibleDC(NULL);

	::EnumFontFamiliesW(hdc, NULL, _enumFamCallBack, (LPARAM)this);

	fontLoadingState = FontLoadingState_LoadMetric;
	return reSchedule(false, ~taskPool->mainThreadId());
}

void FontLoader::loadMetric(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	// Get the font name from the uri
	String fontName;
	{	const char* p = roStrrChr(font->uri(), '.');
		if(!p) { roAssert(false); roEXCP_THROW; }
		fontName.assign(font->uri(), p - font->uri());
	}

	// http://stackoverflow.com/questions/6595772/painting-text-above-opengl-context-in-mfc

	// http://msdn.microsoft.com/en-us/library/windows/desktop/dd183499%28v=vs.85%29.aspx
	hFont = ::CreateFontW(30,0,0,0,FW_DONTCARE,FALSE,FALSE,0,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
//		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"DFKai-SB");
		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Arial");

	::SelectObject(hdc, hFont);

	// Get the unicode range
	Array<char> glyphSet(::GetFontUnicodeRanges(hdc, NULL));
	GLYPHSET* pGlyphSet = glyphSet.castedPtr<GLYPHSET>();
	::GetFontUnicodeRanges(hdc, pGlyphSet);

	// Get text metric
	::GetTextMetricsW(hdc, &tm);

	// Get the kerning table
	DWORD pairCount = ::GetKerningPairsW(hdc, 0, NULL);
	Array<KERNINGPAIR> kerningPair(pairCount);
	::GetKerningPairsW(hdc, pairCount, kerningPair.typedPtr());

	font->kerningPairs.reserve(pairCount);
	for(roSize i=0; i<pairCount; ++i) {
		if(kerningPair[i].iKernAmount == 0)
			continue;
		KerningPair k = { kerningPair[i].wFirst, kerningPair[i].wSecond, kerningPair[i].iKernAmount };
		font->kerningPairs.pushBack(k);
	}

	// Fill in the data structures
	for(roSize i=0; i<pGlyphSet->cRanges; ++i)
	{
		WCRANGE& range = pGlyphSet->ranges[i];
		roSize k = 0;

		for(roSize j=0; j<range.cGlyphs; ++j)
		{
			unsigned codePoint = range.wcLow + j;
			Glyph g = { codePoint, 0, 0, 0, 0, 0, Vec2(0.f), Vec2(0.f) };

			// Find the kerning for this code point
			roSize kBeg = k;
			for(; k<font->kerningPairs.size(); ++k) {
				if(font->kerningPairs[k].first == codePoint)
					g.kerningIndex = num_cast<roUint16>(kBeg);
				else if(font->kerningPairs[k].first > codePoint)
					break;
			}
			g.kerningCount = num_cast<roUint16>(k - kBeg);

			// Set the width and height
			INT w;
			roVerify(::GetCharWidth32W(hdc, codePoint, codePoint, &w));	// NOTE: Use GetCharABCWidthsW if necessary
			g.width = num_cast<roUint16>(w);
			g.height = num_cast<roUint16>(tm.tmHeight);
			roAssert(g.texSize.x <= tm.tmMaxCharWidth);

			font->glyphs.pushBack(g);
		}
	}

	font->kerningPairs.condense();
	font->glyphs.condense();

	// Calculate the optimal number of texture to use, base on the the average glyph width and fixed glyph height
	roSize pixelNeeded = tm.tmAveCharWidth * tm.tmHeight * font->glyphs.size();

	texDimension = 1;
	for(roSize i=1; i<=10; ++i) {
		texDimension = (1 << i);
		unsigned pixels = (1 << i) * (1 << i);
		if(pixels > pixelNeeded)
			break;
	}
	roAssert(texDimension <= 1024);

	fontLoadingState = FontLoadingState_LoadBitmaps;
	return reSchedule(false, ~taskPool->mainThreadId());

roEXCP_CATCH
	roLog("error", "FontLoader: Fail to load '%s', reason: %s\n", font->uri().c_str(), st.c_str());
	fontLoadingState = FontLoadingState_RequestAbort;

roEXCP_END
}

// Copy a texture from one memory to another memory
// TODO: Move to texture utilities
static void texBlit(
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

void FontLoader::loadBitmaps(TaskPool* taskPool)
{
	Status st;

	roEXCP_TRY
	// Get the bitmap data
	ro::Array<char> glyhpBitmapBuf;

	for(; currentGlyphIdx<font->glyphs.size(); ++currentGlyphIdx)
	{
		Glyph& g = font->glyphs[currentGlyphIdx];
		unsigned codePoint = g.codePoint;

		// Get the glyph bitmaps
		// Reference: http://opensource.apple.com/source/WebCore/WebCore-1C25/platform/cairo/cairo/src/cairo-win32-font->c
		// http://www.winehq.org/pipermail/wine-patches/2002-July/002790.html
		GLYPHMETRICS glyphMetrics;
		static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
		roSize bufSize = ::GetGlyphOutlineW(hdc, codePoint, GGO_GRAY8_BITMAP, &glyphMetrics, 0, NULL, &matrix);
		if(GDI_ERROR == bufSize) {
			roLog("warn", "GetGlyphOutlineW for code point %s failed\n", codePoint);
			continue;
		}

		// For some character (like tab and space), there is no need for a bitmap representation
		if(bufSize == 0)
			continue;

		// Get the glyph outline
		glyhpBitmapBuf.resize(bufSize);
		::GetGlyphOutlineW(hdc, codePoint, GGO_GRAY8_BITMAP, &glyphMetrics, bufSize, glyhpBitmapBuf.bytePtr(), &matrix);
		bufSize = bufSize;

		// Scale up from GGO_GRAY8_BITMAP to 0 - 255
		for(roSize i=0; i<glyhpBitmapBuf.size(); ++i) {
			int c = glyhpBitmapBuf[i];
			c = int(c * (float)255 / 64);
			glyhpBitmapBuf[i] = num_cast<char>(c);
		}

		// Increment the position in the destination texture
		// TODO: Add border
		if(dstX + glyphMetrics.gmBlackBoxX > texDimension) {
			dstX = 0;
			dstY += tm.tmHeight;
		}

		// Condition for allocating new texture
		bool texturesEmpty = currentTextureIdx == font->textures.size();
		bool textureFull = dstY + tm.tmHeight > texDimension;
		if(texturesEmpty || textureFull) {
			fontLoadingState = FontLoadingState_AllocateTexture;
			return reSchedule(false, taskPool->mainThreadId());
		}

		// Copy the outline bitmap to the texture atlas
		roSize rowLen = roAlignCeiling(glyphMetrics.gmBlackBoxX, 4u);
		texBlit(1,
			glyhpBitmapBuf.bytePtr(), 0, 0, glyphMetrics.gmBlackBoxX, glyphMetrics.gmBlackBoxY, rowLen,
			mappedTexPtr, dstX, dstY, texDimension, texDimension, texDimension * 1);

		dstX += glyphMetrics.gmBlackBoxX;
	}	// end for each glyphs

	fontLoadingState = FontLoadingState_Cleanup;
	return reSchedule(false, taskPool->mainThreadId());

roEXCP_CATCH
	roLog("error", "FontLoader: Fail to load '%s', reason: %s\n", font->uri().c_str(), st.c_str());
	fontLoadingState = FontLoadingState_RequestAbort;

roEXCP_END
}

static TexturePtr _allocateTexture(unsigned dimension, char** outMappedPtr)
{
	static const unsigned bytePerPixel = 1;
	TexturePtr texture = new Texture("");
	texture->handle = roRDriverCurrentContext->driver->newTexture();

	bool ok = roRDriverCurrentContext->driver->initTexture(
		texture->handle, dimension, dimension, 1, roRDriverTextureFormat_A, roRDriverTextureFlag_None);
	if(!ok) {
		// TODO: More error handling
		roLog("error", "Fail to create texture for font\n");
		return NULL;
	}

	roVerify(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, NULL, 0, NULL));

	if(outMappedPtr) {
		*outMappedPtr = (char*)roRDriverCurrentContext->driver->mapTexture(
			texture->handle, roRDriverMapUsage_Write, 0, 0);
		roZeroMemory(*outMappedPtr, dimension * dimension * bytePerPixel);
	}

	return texture;
}

void FontLoader::allocateTexture(TaskPool* taskPool)
{
	if(dstY + tm.tmHeight > texDimension) {
		dstX = dstY = 0;
		roRDriverCurrentContext->driver->unmapTexture(font->textures[currentTextureIdx]->handle, 0, 0);
		++currentTextureIdx;
	}

	if(currentTextureIdx == font->textures.size()) {
		TexturePtr tex = _allocateTexture(texDimension, &mappedTexPtr);
		// If we fail to allocate texture, seems not much we can do
		if(!tex || !mappedTexPtr) {
			fontLoadingState = FontLoadingState_RequestAbort;
			return;
		}
		font->textures.pushBack(tex);
	}

	fontLoadingState = FontLoadingState_LoadBitmaps;
	return reSchedule(false, ~taskPool->mainThreadId());
}

void FontLoader::requestAbort(TaskPool* taskPool)
{
	font->state = Resource::Aborted;
	fontLoadingState = FontLoadingState_Cleanup;
	return reSchedule(false, taskPool->mainThreadId());
}

void FontLoader::cleanup(TaskPool* taskPool)
{
	if(mappedTexPtr) {
		roRDriverCurrentContext->driver->unmapTexture(font->textures[currentTextureIdx]->handle, 0, 0);
		mappedTexPtr = NULL;
	}

	if(font->state == Resource::Aborted)
		fontLoadingState = FontLoadingState_Abort;
	else
		fontLoadingState = FontLoadingState_Finish;
}

bool resourceLoadWinfnt(Resource* resource, ResourceManager* mgr)
{
	if(roStrCaseCmp("win.fnt", resource->uri()) != 0) return false;

	TaskPool* taskPool = mgr->taskPool;

	FontImpl* font = dynamic_cast<FontImpl*>(resource);

	FontLoader* loaderTask = new FontLoader(font, mgr);
	font->taskReady = font->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool FontImpl::setStyle(const char* styleStr)
{
	return true;
}

void FontImpl::draw(const char* utf8Str, float x, float y, float maxWidth, Canvas& c)
{
//	canvas.setFillColor(1, 1, 1, 1);
//	canvas.setStrokeColor(1, 1, 1, 1);

	// NOTE: We don't call canvas.beginDraw(), to preserve the current render target and view port settings
	if(state == Loaded && !textures.isEmpty())
		c.drawImage(textures[0]->handle, 0, 0);
}

}	// namespace ro
