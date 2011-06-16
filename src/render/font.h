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
	/// 
	virtual bool bake(unsigned fontIdx, unsigned fontSize) = 0;

	virtual void getMetrics(int* ascent, int* descent, int* lineGap) {}

// Attributes
};	// Font

typedef IntrusivePtr<Font> FontPtr;

}	// namespace Render

#endif	// __RENDER_FONT_H__
