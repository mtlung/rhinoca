#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#include "../common.h"
#include "../resource.h"

namespace Render {

class Texture : public Resource
{
public:
	explicit Texture(const char* uri);
	virtual ~Texture();

	// Constants from gl.h
	enum Format {
		RGB		= 0x1907,
		BGR		= 0x80E0,
		RGBA	= 0x1908,
		BGRA	= 0x80E1,
	};

// Operations
	bool create(rhuint width, rhuint height, const char* data, rhuint dataSize, int srcFormat);

	void clear();

	void bind();

// Attibutes
	rhuint handle;
	rhuint width, height;

protected:
	rhuint _type;
};	// Texture

typedef IntrusivePtr<Texture> TexturePtr;

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
