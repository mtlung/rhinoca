#ifndef __render_roTexture_h__
#define __render_roTexture_h__

#include "../base/roResource.h"

struct roRDriverTexture;

namespace ro {

struct Texture : public ro::Resource
{
	explicit Texture(const char* uri);
	virtual ~Texture();

// Attributes
	override ConstString resourceType() const { return "Texture"; }

	/// Logical width/height, useful in 2D rendering
	unsigned width;
	unsigned height;

	/// Width/height from the raw texture data
	unsigned actualWidth() const;
	unsigned actualHeight() const;

	roRDriverTexture* handle;
};	// Texture

typedef ro::SharedPtr<Texture> TexturePtr;

}	// namespace ro

void roTextureBlit(
	unsigned bytePerPixel,
	const char* srcPtr, unsigned srcX, unsigned srcY, unsigned srcWidth, unsigned srcHeight, unsigned srcRowBytes,
	      char* dstPtr, unsigned dstX, unsigned dstY, unsigned dstWidth, unsigned dstHeight, unsigned dstRowBytes
);


#endif	// __render_roTexture_h__
