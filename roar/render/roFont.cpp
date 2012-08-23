#include "pch.h"
#include "roFont.h"
#include "roRenderDriver.h"
#include "../base/roParser.h"
#include "../roSubSystems.h"

namespace ro {

TextMetrics::TextMetrics()
	: width(0), height(0)
{
}

Font::Font(const char* uri)
	: Resource(uri)
{
}

Font::~Font()
{
}

void FontManager::registerFontTypeface(const char* typeface, const char* fontUri)
{
	_FontTypeface f = { fontUri, typeface };
	_mapping.pushBack(f);
}

FontPtr FontManager::getFont(const char* typeface)
{
	if(!roSubSystems || !roSubSystems->resourceMgr)
		return NULL;

	StringHash h = stringLowerCaseHash(typeface);
	for(roSize i=0; i<_mapping.size(); ++i) {
		if(_mapping[i].typeface.lowerCaseHash() == h)
			return roSubSystems->resourceMgr->loadAs<Font>(_mapping[i].fontUri.c_str());
	}
	return NULL;
}

namespace Parsing {

// Reference: http://www.w3.org/TR/css3-fonts

bool FontFamilyMatcher::match(Parser* p)
{
	whiteSpace(p).any();
	return 
		doubleQuotedString(p).once() ||
		anyCharExcept(p, " ,\t\n\r").atLeastOnce();
}

Matcher<FontFamilyMatcher> fontFamily(Parser* parser) {
	Matcher<FontFamilyMatcher> ret = { {}, parser };
	return ret;
}

bool FontStyleMatcher::match(Parser* p)
{
	whiteSpace(p).any();
	return
		string(p, "normal").once() ||
		string(p, "italic").once() ||
		string(p, "oblique").once();
}

Matcher<FontStyleMatcher> fontStyle(Parser* parser) {
	Matcher<FontStyleMatcher> ret = { {}, parser };
	return ret;
}

bool FontWeightMatcher::match(Parser* p)
{
	whiteSpace(p).any();
	Transcation t(p);

	bool ret =
		(string(p, "normal").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "bold").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "100").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "200").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "300").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "400").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "500").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "600").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "700").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "800").once() && whiteSpace(p).atLeastOnce()) ||
		(string(p, "900").once() && whiteSpace(p).atLeastOnce());
	if(!ret) return false;

	t.commit();
	return true;
}

Matcher<FontWeightMatcher> fontWeight(Parser* parser) {
	Matcher<FontWeightMatcher> ret = { {}, parser };
	return ret;
}

bool FontSizeMatcher::match(Parser* p)
{
	whiteSpace(p).any();
	bool ret =
		string(p, "xx-small").once() ||
		string(p, "x-small").once() ||
		string(p, "small").once() ||
		string(p, "medium").once() ||
		string(p, "large").once() ||
		string(p, "x-large").once() ||
		string(p, "xx-large").once() ||

		string(p, "larger").once() ||
		string(p, "smaller").once();
	if(ret) return true;

	Transcation t(p);
	ret =
		(t.rollback(), digit(p).atLeastOnce() && string(p, "pt").once()) ||
		(t.rollback(), digit(p).atLeastOnce() && character(p, '%').once()) ||
		(t.rollback(), number(p).once() && string(p, "em").once());
	if(!ret) return false;

	t.commit();
	return true;
}

Matcher<FontSizeMatcher> fontSize(Parser* parser) {
	Matcher<FontSizeMatcher> ret = { {}, parser };
	return ret;
}

bool LineHeightMatcher::match(Parser* p)
{
	whiteSpace(p).any();

	Transcation t(p);
	bool ret =
		(t.rollback(), digit(p).atLeastOnce() && string(p, "pt").once()) ||
		(t.rollback(), digit(p).atLeastOnce() && character(p, '%').once()) ||
		(t.rollback(), number(p).once() && string(p, "em").once());
	if(!ret) return false;

	t.commit();
	return true;
}

Matcher<LineHeightMatcher> lineHeight(Parser* parser) {
	Matcher<LineHeightMatcher> ret = { {}, parser };
	return ret;
}

bool FontStretchMatcher::match(Parser* p)
{
	whiteSpace(p).any();
	return
		string(p, "normal").once() ||
		string(p, "ultra-condensed").once() ||
		string(p, "extra-condensed").once() ||
		string(p, "condensed").once() ||
		string(p, "semi-condensed").once() ||
		string(p, "semi-expanded").once() ||
		string(p, "expanded").once() ||
		string(p, "extra-expanded").once() ||
		string(p, "ultra-expanded ").once();
}

Matcher<FontStretchMatcher> fontStretch(Parser* parser) {
	Matcher<FontStretchMatcher> ret = { {}, parser };
	return ret;
}

// NOTE: For this we need a parser with back tracking support
// http://pauillac.inria.fr/~ddr/camlp5/doc/htmlc/bparsers.html
// For example: "100pt Calibri" should be reconized as fontSize and fontFamily,
// but without back tracking, the first "100" will reconized as fontWeight
bool FontMatcher::match(Parser* p)
{
	whiteSpace(p).any();

	Transcation t(p);

	ParserResult fontStyleResult = { "fontStyle", NULL, NULL };
	ParserResult fontWeightResult = { "fontWeight", NULL, NULL };

	fontStyle(p).once(&fontStyleResult);
	fontWeight(p).once(&fontWeightResult);

	ParserResult fontSizeResult = { "fontSize", NULL, NULL };
	if(!fontSize(p).once(&fontSizeResult))
		return false;

	ParserResult lineHeightResult = { "lineHeight", NULL, NULL };
	{	Transcation t2(p);
		if(whiteSpace(p).any() && character(p, '/').once() && whiteSpace(p).any() && lineHeight(p).once(&lineHeightResult))
			t2.commit();
	}

	ParserResult fontFamilyResult = { "fontFamily", NULL, NULL };
	if(!whiteSpace(p).any() || !fontFamily(p).once(&fontFamilyResult))
		return false;

	t.commit();
	return true;
}

Matcher<FontMatcher> font(Parser* parser) {
	Matcher<FontMatcher> ret = { {}, parser };
	return ret;
}

}	// namespace Parsing

}	// namespace ro
