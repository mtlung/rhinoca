#ifndef __render_roTexture_h__
#define __render_roTexture_h__

//#include "roRenderDriver.h"
#include "../base/roResource.h"

struct roRDriverTexture;

namespace ro {

struct Texture : public ro::Resource
{
	explicit Texture(const char* uri);
	virtual ~Texture();

// Attributes
	roRDriverTexture* handle;
};	// Texture

typedef ro::SharedPtr<Texture> TexturePtr;

}	// namespace Render

#endif	// __render_roTexture_h__
