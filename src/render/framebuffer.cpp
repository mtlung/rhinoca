#include "pch.h"
#include "framebuffer.h"
#include "driver.h"

namespace Render {

Framebuffer::Framebuffer()
	: handle(0)
	, depthHandle(0), stencilHandle(0)
	, width(0), height(0)
{
}

Framebuffer::~Framebuffer()
{
	if(texture) Driver::deleteRenderTarget(handle);
	// TODO: Delete depthHandle and stencilHandle
}

void Framebuffer::bind()
{
	Driver::useRenderTarget(handle);
}

void Framebuffer::unbind()
{
	Driver::useRenderTarget(NULL);
}

void Framebuffer::createTexture(unsigned w, unsigned h)
{
	if(width == w && height == h) return;

	texture = new Texture("");
	(void)texture->create(w, h, Render::Driver::ANY, NULL, 0, Render::Driver::RGBA);
	width = w;
	height = h;
	Driver::deleteRenderTarget(handle);
	handle = Driver::createRenderTargetTexture(&texture->handle, &depthHandle, &stencilHandle, w, h);
}

}	// Render

// Opengl render context
struct RhinocaRenderContext
{
	void* platformSpecificContext;
	unsigned fbo;
	unsigned depth;
	unsigned stencil;
	unsigned texture;
};

void Render::Framebuffer::useExternal(RhinocaRenderContext* context)
{
	texture = NULL;
	handle = (void*)context->fbo;
}
