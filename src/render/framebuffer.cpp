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
	if(texture)
		Driver::deleteRenderTarget(handle, &depthHandle, &stencilHandle);
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

	if(!texture)
		texture = new Texture("");

	width = w;
	height = h;

	// NOTE: Do adjustment to w and h here on devices which have
	// limitation on the size of the FBO.
#ifdef RHINOCA_IOS_DEVICE	
	// NOTE: No idea why iOS device will fail in checking framebuffer completness
	// if the dimension is power of two, this at least happens on my iPod4G
	if(Driver::isPowerOfTwo(w)) ++w;
	if(Driver::isPowerOfTwo(h)) ++h;
#endif

	const unsigned maxTexSize = (unsigned)Driver::getCapability("maxTexSize");
	if(w > maxTexSize) w = maxTexSize;
	if(h > maxTexSize) h = maxTexSize;

	VERIFY(texture->create(w, h, Render::Driver::ANY, NULL, 0, Render::Driver::RGBA));
	texture->virtualWidth = width;
	texture->virtualHeight = height;

	handle = Driver::createRenderTarget(handle, &texture->handle, &depthHandle, &stencilHandle, w, h);

	{	// Set the framebuffer handle as the key of the texture, for debugging purpose
		char buf[128];
		sprintf(buf, "Framebuffer %x", (unsigned)handle);
		texture->setKey(buf);

		printf("creating Framebuffer:%x, w:%d h:%d\n", (unsigned)handle, width, height);
	}
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
