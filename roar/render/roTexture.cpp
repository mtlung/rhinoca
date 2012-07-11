#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"

namespace ro {

Texture::Texture(const char* uri)
	: Resource(uri)
	, width(0), height(0)
	, handle(NULL)
{
}

Texture::~Texture()
{
	roAssert(roRDriverCurrentContext && "Please release all resource before shutdown the system");
	roRDriverCurrentContext->driver->deleteTexture(handle);
}

unsigned Texture::actualWidth() const
{
	return handle ? handle->width : 0;
}

unsigned Texture::actualHeight() const
{
	return handle ? handle->height : 0;
}

}	// namespace ro

/// Copy a texture from one memory to another memory
void roTextureBlit(
	unsigned bytePerPixel,
	const char* srcPtr, unsigned srcX, unsigned srcY, unsigned srcWidth, unsigned srcHeight, unsigned srcRowBytes,
	char* dstPtr, unsigned dstX, unsigned dstY, unsigned dstWidth, unsigned dstHeight, unsigned dstRowBytes)
{
	const char* pSrc = srcPtr;
	char* pDst = dstPtr;
	for(unsigned sy=srcY, dy=dstY; sy < srcY + srcHeight && dy < dstY + dstHeight; ++sy, ++dy) {
		pSrc = srcPtr + srcRowBytes * sy + srcX * bytePerPixel;
		pDst = dstPtr + dstRowBytes * dy + dstX * bytePerPixel;

		unsigned rowLen = roMinOf2(srcWidth - srcX, dstWidth - dstX);
		roMemcpy(pDst, pSrc, rowLen * bytePerPixel);
	}
}
