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
	roAssert(roRDriverCurrentContext && "Please release all resource before shutdown the system");
	roRDriverCurrentContext->driver->deleteTexture(handle);
}

unsigned Texture::width() const
{
	return handle ? handle->width : 0;
}

unsigned Texture::height() const
{
	return handle ? handle->height : 0;
}

}	// namespace ro
