#ifndef __RENDERER_FRAMEBUFFER_H__
#define __RENDERER_FRAMEBUFFER_H__

#include "texture.h"
#include "../common.h"

namespace Render {

class Framebuffer : private ro::NonCopyable
{
public:
	Framebuffer();
	~Framebuffer();

// Operation
	void bind();
	void unbind();

	void createTexture(unsigned width, unsigned height);

	void useExternal(RhinocaRenderContext* context);

// Attributes
	void* handle;
	void* depthHandle, *stencilHandle;

	unsigned width, height;

	/// Null -> Render to frame buffer
	/// Not null -> Render to texture
	TexturePtr texture;
};	// Framebuffer

}	// Render

#endif	// __RENDERER_FRAMEBUFFER_H__
