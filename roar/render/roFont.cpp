#include "pch.h"
#include "roFont.h"
#include "roRenderDriver.h"
#include "../roSubSystems.h"

namespace ro {

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

}	// namespace ro
