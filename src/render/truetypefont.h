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
	override bool bake(unsigned fontIdx, unsigned fontSize);

	override void getMetrics(int* ascent, int* descent, int* lineGap);

	// Attributes

	class Impl;
	Impl* impl;
};	// Font

}	// namespace Render

#endif	// __RENDER_TRUETYPEFONT_H__
