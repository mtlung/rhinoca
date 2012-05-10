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

	roRDriverTexture* handle;
};	// Texture

typedef ro::SharedPtr<Texture> TexturePtr;

}	// namespace ro

#endif	// __render_roTexture_h__
