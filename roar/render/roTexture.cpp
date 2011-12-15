#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"

namespace ro {

Texture::Texture(const char* uri)
	: Resource(uri)
	, handle(NULL)
{
}

Texture::~Texture()
{
	roRDriverCurrentContext->driver->deleteTexture(handle);
}

}	// namespace ro
