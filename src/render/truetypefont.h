#ifndef __RENDER_TRUETYPEFONT_H__
#define __RENDER_TRUETYPEFONT_H__

#include "font.h"

namespace Render {

class TrueTypeFont : public Font
{
public:
	explicit TrueTypeFont(const char* uri);
	override ~TrueTypeFont();

	// Operations
	override rhuint8* bake(unsigned fontIdx, unsigned fontPixelHeight, int codepoint, int* width, int* height, int* xoff, int* yoff);

	override void freeBitmap(rhuint8* bitmap);

	override void getMetrics(int* ascent, int* descent, int* lineGap);

	// Attributes

	class Impl;
	Impl* impl;
};	// Font

}	// namespace Render

#endif	// __RENDER_TRUETYPEFONT_H__
