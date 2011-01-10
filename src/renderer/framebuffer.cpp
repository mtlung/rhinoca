#include "pch.h"
#include "framebuffer.h"
#include "glew.h"

namespace Render {

Framebuffer::Framebuffer()
{
	glGenFramebuffers(1, (GLuint*)&handle);
}

Framebuffer::~Framebuffer()
{
	if(handle) glDeleteFramebuffers(1, (GLuint*)&handle);
}

}	// Render
