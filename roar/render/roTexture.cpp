#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"

namespace ro {

Texture::Texture(const char* uri)
	: Resource(uri)
	, handle(NULL)
	, _width(unsigned(-1)) , _height(unsigned(-1))
{
}

Texture::~Texture()
{
	roAssert(roRDriverCurrentContext && "Please release all resource before shutdown the system");
	roRDriverCurrentContext->driver->deleteTexture(handle);
}

unsigned Texture::width() const
{
	if(_width == unsigned(-1))
		return actualWidth();
	return _width;
}

unsigned Texture::height() const
{
	if(_height == unsigned(-1))
		return actualHeight();
	return _height;
}

void Texture::setWidth(unsigned w)
{
	_width = w;
}

void Texture::setHeight(unsigned h)
{
	_height = h;
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
	roSize bytePerPixel, unsigned dirtyWidth, unsigned dirtyHeight,
	const char* srcPtr, unsigned srcX, unsigned srcY, unsigned srcHeight, roSize srcRowBytes, bool srcYUp,
		  char* dstPtr, unsigned dstX, unsigned dstY, unsigned dstHeight, roSize dstRowBytes, bool dstYUp)
{
	const char* pSrc = srcPtr;
	char* pDst = dstPtr;

	// Unify the coordinate in y-down
	srcY = srcYUp ? srcHeight - srcY - 1: srcY;
	dstY = dstYUp ? dstHeight - dstY - 1 : dstY;
	int src_dy = srcYUp ? -1 : 1;
	int dst_dy = dstYUp ? -1 : 1;

	if(false && srcYUp == dstYUp && srcX == 0 && dstX == 0 && srcRowBytes == dstRowBytes) {
		pSrc = srcPtr + srcRowBytes * srcY;
		pDst = dstPtr + dstRowBytes * dstY;
		unsigned height = roMinOf2(srcHeight, dstHeight);
		roMemcpy(pDst, pSrc, srcRowBytes * height);
	}
	else {
		for(unsigned sy=srcY, dy=dstY; dirtyHeight; --dirtyHeight) {
			pSrc = srcPtr + srcRowBytes * sy + srcX * bytePerPixel;
			pDst = dstPtr + dstRowBytes * dy + dstX * bytePerPixel;

			roMemcpy(pDst, pSrc, dirtyWidth * bytePerPixel);

			sy += src_dy;
			dy += dst_dy;
		}
	}
}
