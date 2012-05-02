#ifndef __render_roFont_h__
#define __render_roFont_h__

#include "../base/roArray.h"
#include "../base/roResource.h"

namespace ro {

struct Canvas;

/// A Font resource contain a set of typeface
struct Font : public ro::Resource
{
	explicit Font(const char* uri);
	virtual ~Font();

	/// Affect the font style for later draw command
	/// Use CSS font style string: http://www.w3schools.com/css/css_font.asp
	/// Format:
	///	font-family : "Times New Roman" | ...,
	///	font-style : "normal" | "italic",
	/// font-weight : "normal" | "bold"
	///	font-size : 40px | 1.875em,
	virtual bool setStyle(const char* styleStr) { return false; }

	/// Draw to the roRDriverCurrentContext, with it current selected render target
	virtual void draw(const char* utf8Str, float x, float y, float maxWidth, Canvas& canvas) {}

// Attributes
};	// Texture

typedef ro::SharedPtr<Font> FontPtr;

/// Map font typeface name to Font resource
struct FontManager
{
	/// Called by font loader to register the typefaces it contains
	void registerFontTypeface(const char* typeface, const char* fontUri);

	FontPtr getFont(const char* typeFace);

// Private
	struct _FontTypeface {
		ConstString fontUri;
		ConstString typeface;
	};
	Array<_FontTypeface> _mapping;
};	// FontManager

}	// namespace ro

#endif	// __render_roFont_h__
