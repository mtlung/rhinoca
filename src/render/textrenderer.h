#ifndef __RENDER_TEXTRENDERER_H__
#define __RENDER_TEXTRENDERER_H__

#include "font.h"

namespace Render {

/// Some references:
/// http://gpwiki.org/index.php/OpenGL:Tutorials:Font_System
/// iPhone text rendering using:
/// http://www.idevgames.com/forums/thread-1022.html
class TextRenderer
{
public:
	TextRenderer();
	~TextRenderer();

// Operations
	/// For fast text rendering, the font settings and the string will be encoded
	/// into a single hash value, and later you use this hash for drawing the text
	/// multiple times with different location on the screen, different color etc.
	/// The cache will be evicted if it's not used in several frames.
	rhuint32 makeHash(Font* font, unsigned fontPixelHeight, const char* utf8Str, unsigned strLen=0);

	/// x and y represents the upper-left corner of the drawn text.
	/// Returns false if the hash is not valid (uncached already)
	bool drawText(rhuint32 hash, float x, float y, float* colorFloat4);

	rhuint32 drawText(Font* font, unsigned fontPixelHeight, const char* utf8Str, unsigned strLen, float x, float y, float* colorFloat4);

	void addReference(rhuint32 hash);
	void releaseReference(rhuint32 hash);

	/// Periodically call this function for cache management.
	void update();

protected:
	class Impl;
	Impl* impl;
};	// TextRenderer

}	// namespace Render

#endif	// __RENDER_TEXTRENDERER_H__
