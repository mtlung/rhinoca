#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#include "driver.h"
#include "../common.h"
#include "../resource.h"

namespace Render {

class Texture : public Resource
{
public:
	explicit Texture(const char* uri);
	virtual ~Texture();

// Operations
	typedef Driver::TextureFormat Format;

	bool create(unsigned width, unsigned height, Format internalFormat, const char* data, unsigned dataSize, Format dataFormat);

	void clear();

// Attributes
	void* handle;
	unsigned width, height;

	/// For reason of dealing with non power of 2 texture,
	/// some texture might enlarged beyond it's necessary.
	/// This uv width and height specify the extend in uv.
	float uvWidth, uvHeight;
};	// Texture

typedef IntrusivePtr<Texture> TexturePtr;

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
