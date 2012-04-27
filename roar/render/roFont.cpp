#include "pch.h"
#include "roFont.h"
#include "roRenderDriver.h"

namespace ro {

Font::Font(const char* uri)
	: Resource(uri)
{
}

Font::~Font()
{
}

}	// namespace ro
