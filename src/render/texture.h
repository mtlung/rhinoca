#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#include "driver.h"
#include "../resource.h"

namespace Render {

class Texture : public Resource
{
public:
	explicit Texture(const char* uri);
	virtual ~Texture();

// Operations
	typedef Driver::TextureFormat Format;

	bool create(unsigned width, unsigned height, Format internalFormat, const char* data, unsigned dataSize, Format dataFormat, unsigned packAlignment=1);

	void clear();

// Attributes
	void* handle;
	unsigned width, height;

	/// Pretending the texture is of this size, useful when dealing with some
	/// device which have limitations on the actual texture size.
	/// If the virtual and actual size are not the same, the texture is cliped or padded but not strached.
	unsigned virtualWidth, virtualHeight;
};	// Texture

typedef IntrusivePtr<Texture> TexturePtr;

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
