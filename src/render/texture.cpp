#include "pch.h"
#include "texture.h"

namespace Render {

Texture::Texture(const char* uri)
	: Resource(uri)
	, handle(0)
	, width(0), height(0)
{
}

Texture::~Texture()
{
	clear();
}

bool Texture::create(unsigned w, unsigned h, const char* data, unsigned dataSize, Format dataFormat)
{
	width = w;
	height = h;

	if(w == 0 || h == 0) return false;

	handle = Driver::createTexture(w, h, Driver::ANY, data, dataFormat);

	return true;
}

void Texture::clear()
{
	Driver::deleteTexture(handle);
	handle = 0;
	width = height = 0;
}

void Texture::bind()
{
	if(handle) {
		++hotness;
		Driver::useTexture(handle);
	}
}

}
