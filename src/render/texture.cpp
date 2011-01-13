#include "pch.h"
#include "texture.h"
#include "glew.h"

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

static rhuint formatComponentCount(int format)
{
	switch(format) {
	case GL_RGB: return 3;
	case GL_BGR: return 3;
	case GL_RGBA: return 4;
	case GL_BGRA: return 4;
	}
	ASSERT(false);
	return 0;
}

bool Texture::create(rhuint w, rhuint h, const char* data, rhuint dataSize, int srcFormat)
{
	_type = GL_TEXTURE_2D;
	width = w;
	height = h;

	glGenTextures(1, &handle);
	glBindTexture(_type, handle);

	glTexImage2D(
		_type, 0, GL_RGBA8, width, height, 0,
		formatComponentCount(srcFormat),
		srcFormat,
		data
	);

	return true;
}

void Texture::clear()
{
	// NOTE: glDeleteTextures will simple ignore non-valid texture handles
	// but may fail to handle with handle = 0 in my Win7 64-bit
	// So I added an explicit check
	if(handle) glDeleteTextures(1, &handle);
	handle = 0;
	_type = 0;
	width = height = 0;
}

}
