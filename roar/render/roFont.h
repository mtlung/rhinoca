#ifndef __render_roFont_h__
#define __render_roFont_h__

#include "../base/roArray.h"
#include "../base/roResource.h"

namespace ro {

/// A Font resource contain a set of typeface
struct Font : public ro::Resource
{
	explicit Font(const char* uri);
	virtual ~Font();

	/// Affect the font style for later draw command
	virtual bool setStyle(const char* styleStr);

	/// Draw to the roRDriverCurrentContext, with it current selected render target
	virtual void draw(unsigned x, unsigned y, const char* utf8Str);

// Attributes
};	// Texture

typedef ro::SharedPtr<Font> FontPtr;

/// Map font typeface name to Font resource
struct FontManager
{
	FontManager(ResourceManager* resourceMgr);

	/// Called by font loader to register the typefaces it contains
	void addTypefaceForFont(const char* typeface, const char* fontUri);

	FontPtr getFont(const char* typeFace);

// Private
	struct Pair {
		ConstString typeface;
		FontPtr font;
	};
	Array<Pair> mapping;
};	// FontManager

}	// namespace ro

#endif	// __render_roFont_h__
