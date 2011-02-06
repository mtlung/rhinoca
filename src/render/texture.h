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

	bool create(unsigned width, unsigned height, const char* data, unsigned dataSize, Format dataFormat);

	void clear();

	void bind();

// Attibutes
	void* handle;
	unsigned width, height;
};	// Texture

typedef IntrusivePtr<Texture> TexturePtr;

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
