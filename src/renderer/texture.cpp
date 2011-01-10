#include "pch.h"
#include "texture.h"
#include "glew.h"

namespace Render {

Texture::Texture()
	: handle(0)
	, _width(0), _height(0)
{
}

Texture::~Texture()
{
	clear();
}

static rhuint formatComponentCount(Texture::Format format)
{
	switch(format) {
	case Texture::RGB: return 3;
	case Texture::BGR: return 3;
	case Texture::RGBA: return 4;
	case Texture::BGRA: return 4;
	}
	ASSERT(false);
	return 0;
}

static rhuint formatOglFormat(Texture::Format format)
{
	switch(format) {
	case Texture::RGB: return GL_RGB;
	case Texture::BGR: return GL_BGR;
	case Texture::RGBA: return GL_RGBA;
	case Texture::BGRA: return GL_BGRA;
	}
	ASSERT(false);
	return 0;
}

bool Texture::create(rhuint width, rhuint height, const char* data, rhuint dataSize, Format dataFormat)
{
	_type = GL_TEXTURE_2D;
	_width = width;
	_height = height;

	glGenTextures(1, &handle);
	glBindTexture(_type, handle);

	glTexImage2D(
		_type, 0, GL_RGBA8, width, height, 0,
		formatComponentCount(dataFormat),
		formatOglFormat(dataFormat),
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
	_width = _height = 0;
}

}
