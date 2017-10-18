#ifndef __render_roFont_h__
#define __render_roFont_h__

#include "../base/roArray.h"
#include "../base/roResource.h"

namespace ro {

struct Canvas;

/// Information about a block of text
struct TextMetrics
{
	TextMetrics();

	float	width;
	float	height;
	float	lineSpacing;
	roSize	lineCount;
};

enum TextAlignment
{
	TextAlignment_Center,
	TextAlignment_Start,	/// Start of the text will align to the anchor point
	TextAlignment_End,		/// End of the text will align to the anchor point
	TextAlignment_Left,		/// Align the left of the string to the anchor point, ignoring whether the text is left-right or right-left
	TextAlignment_Right,	/// Align the right of the string to the anchor point, ignoring whether the text is left-right or right-left
};

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

	/// Will fail if the font is not yet loaded, you may need to loop until it success
	/// You can specify the initial width and height by passing though the "metrics" parameter
	virtual roStatus measure(const roUtf8* str, roSize maxStrLen, float maxWidth, TextMetrics& metrics) const { return roStatus::not_implemented; }

	virtual float getLineSpacing() const { return 0; }

	/// Draw to the roRDriverCurrentContext, with it current selected render target
	virtual void draw(const roUtf8* str, roSize maxStrLen, float x, float y, float maxWidth, Canvas& canvas) {}

// Attributes
	ConstString resourceType() const override { return "Font"; }
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
	Mutex _mutex;
};	// FontManager

namespace Parsing {

// A bunch of stuff to parse the CSS font properties
struct Parser;
template<typename T> struct Matcher;

/// normal | italic | oblique
struct FontStyleMatcher {
	bool match(Parser* p);
};
Matcher<FontStyleMatcher> fontStyle(Parser* parser);

/// normal | italic | oblique
struct FontFamilyMatcher {
	bool match(Parser* p);
};
Matcher<FontFamilyMatcher> fontFamily(Parser* parser);

/// normal | bold | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 
struct FontWeightMatcher {
	bool match(Parser* p);
};
Matcher<FontWeightMatcher> fontWeight(Parser* parser);

/// digits pt | digits % | number em | xx-small | x-small | small | medium | large | x-large | xx-large | larger | smaller
struct FontSizeMatcher {
	bool match(Parser* p);
};
Matcher<FontSizeMatcher> fontSize(Parser* parser);

/// digits pt | digits %
struct LineHeightMatcher {
	bool match(Parser* p);
};
Matcher<LineHeightMatcher> lineHeight(Parser* parser);

/// normal | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded 
struct FontStretchMatcher {
	bool match(Parser* p);
};
Matcher<FontStretchMatcher> fontStretch(Parser* parser);

/// [[fontStyle | fontWeight]? fontSize [/lineHeight]? fontFamily] | caption | icon | menu | message-box 
struct FontMatcher {
	bool match(Parser* p);
};
Matcher<FontMatcher> font(Parser* parser);

}	// namespace Parsing

}	// namespace ro

#endif	// __render_roFont_h__
