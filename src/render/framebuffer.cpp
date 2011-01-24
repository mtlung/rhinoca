#include "pch.h"
#include "framebuffer.h"
#include "glew.h"

namespace Render {

Framebuffer::Framebuffer()
	: texture(NULL)
	, external(0)
	, width(0), height(0)
	, _binded(false)
{
	glGenFramebuffers(1, (GLuint*)&handle);
}

Framebuffer::~Framebuffer()
{
	ASSERT(!_binded);
	if(handle) glDeleteFramebuffers(1, (GLuint*)&handle);
}

void Framebuffer::bind()
{
	if(_binded) return;

	// TODO: Collect framebuffer switching statistic

	ASSERT(GL_NO_ERROR == glGetError());

	if(external)
		glBindFramebuffer(GL_FRAMEBUFFER, external);
	else if(texture) {
		glBindFramebuffer(GL_FRAMEBUFFER, handle);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, texture->handle, 0/*mipmap level*/);

		ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		ASSERT(GL_NO_ERROR == glGetError());
	}
	else
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	_binded = true;
}

void Framebuffer::unbind()
{
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_binded = false;
}

void Framebuffer::createTexture(unsigned w, unsigned h)
{
	texture = new Texture("");
	texture->create(w, h, NULL, 0, Texture::BGRA);
	width = w;
	height = h;
}

}	// Render

// Opengl render context
struct RhinocaRenderContext
{
	unsigned fbo;
	unsigned depth;
	unsigned texture;
};

void Render::Framebuffer::useExternal(RhinocaRenderContext* context)
{
	external = context->fbo;
}
