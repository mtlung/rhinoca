#ifndef __RENDER_FONT_H__
#define __RENDER_FONT_H__

#include "driver.h"
#include "../common.h"
#include "../resource.h"

namespace Render {

/// An abstract font hold implementation/platform specific data for
/// rasterizing font glyphs to memory.
class Font : public Resource
{
protected:
	explicit Font(const char* uri) : Resource(uri) {}

public:
// Operations
	/// Allocates a large-enough single-channel 8bpp bitmap and renders the
	/// specified character/glyph at the specified font size into it, with
	/// antialiasing. 0 is no coverage (transparent), 255 is fully covered (opaque).
	/// *width & *height are filled out with the width & height of the bitmap,
	/// which is stored left-to-right, top-to-bottom.
	/// xoff/yoff are the offset in pixel space from the glyph origin to the top-left of the bitmap
	virtual rhuint8* bake(unsigned fontIdx, unsigned fontPixelHeight, int codepoint, int* width, int* height, int* xoff, int* yoff) = 0;

	/// Free the bitmap created by bake()
	virtual void freeBitmap(rhuint8* bitmap) = 0;

	virtual void getMetrics(int* ascent, int* descent, int* lineGap) {}

// Attributes
};	// Font

typedef IntrusivePtr<Font> FontPtr;

}	// namespace Render

#endif	// __RENDER_FONT_H__
