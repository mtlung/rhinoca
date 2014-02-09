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
	ConstString resourceType() const override { return "Texture"; }

	/// Logical width/height, useful in 2D rendering
	unsigned width() const;
	unsigned height() const;
	void setWidth(unsigned w);
	void setHeight(unsigned h);

	/// Width/height from the raw texture data
	unsigned actualWidth() const;
	unsigned actualHeight() const;

	roRDriverTexture* handle;

// Private
	unsigned _width;
	unsigned _height;
};	// Texture

typedef ro::SharedPtr<Texture> TexturePtr;

}	// namespace ro

/// srcY and dstY are assumed to be Y-down
void roTextureBlit(
	roSize bytePerPixel, unsigned dirtyWidth, unsigned dirtyHeight,
	const char* srcPtr, unsigned srcX, unsigned srcY, unsigned srcHeight, roSize srcRowBytes, bool srcYUp,
	      char* dstPtr, unsigned dstX, unsigned dstY, unsigned dstHeight, roSize dstRowBytes, bool dstYUp
);

#endif	// __render_roTexture_h__
