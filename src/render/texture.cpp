#include "pch.h"
#include "texture.h"

namespace Render {

Texture::Texture(const char* uri)
	: Resource(uri)
	, handle(0)
	, width(0), height(0)
	, virtualWidth(0), virtualHeight(0)
{
}

Texture::~Texture()
{
	clear();
}

bool Texture::create(unsigned w, unsigned h, Format internalFormat, const char* data, unsigned dataSize, Format dataFormat)
{
	width = w;
	height = h;

	virtualWidth = (virtualWidth == 0) ? w : virtualWidth;
	virtualHeight = (virtualHeight == 0) ? h : virtualHeight;

	if(w == 0 || h == 0) return false;

	if(!Driver::getCapability("npot")) {
		width = Driver::nextPowerOfTwo(w);
		height = Driver::nextPowerOfTwo(h);
	}

	handle = Driver::createTexture(handle, width, height, internalFormat, data, dataFormat);

	return true;
}

void Texture::clear()
{
	Driver::deleteTexture(handle);
	handle = 0;
	width = virtualWidth = 0;
	height = virtualHeight = 0;
}

}
