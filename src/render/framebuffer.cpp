#include "pch.h"
#include "framebuffer.h"
#include "driver.h"

namespace Render {

Framebuffer::Framebuffer()
	: handle(0)
	, depthStencilHandle(0)
	, width(0), height(0)
{
}

Framebuffer::~Framebuffer()
{
	if(texture) Driver::deleteRenderTarget(handle);
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
	texture = new Texture("");
	(void)texture->create(w, h, Render::Driver::ANY, NULL, 0, Render::Driver::RGBA);
	width = w;
	height = h;
	Driver::deleteRenderTarget(handle);
	handle = Driver::createRenderTargetTexture(&texture->handle, &depthStencilHandle, w, h);
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
