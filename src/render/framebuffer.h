#ifndef __RENDERER_FRAMEBUFFER_H__
#define __RENDERER_FRAMEBUFFER_H__

#include "../common.h"
#include "texture.h"

namespace Render {

class Framebuffer : private Noncopyable
{
public:
	Framebuffer();
	~Framebuffer();

// Operation
	void bind();
	void unbind();

	void createTexture(unsigned width, unsigned height);

	void useExternal(RhinocaRenderContext* context);

// Attibutes
	rhuint handle;
	rhuint external;

	rhuint width, height;

	/// Null -> Render to frame buffer
	/// Not null -> Render to texture
	TexturePtr texture;

protected:
	bool _binded;
};	// Framebuffer

}	// Render

#endif	// __RENDERER_FRAMEBUFFER_H__
