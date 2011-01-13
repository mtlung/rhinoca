#ifndef __RENDERER_FRAMEBUFFER_H__
#define __RENDERER_FRAMEBUFFER_H__

#include "../common.h"

namespace Render {

class Framebuffer : private Noncopyable
{
public:
	Framebuffer();
	~Framebuffer();

// Attibutes
	rhuint handle;
	rhuint width, height;
};	// Framebuffer

}	// Render

#endif	// __RENDERER_FRAMEBUFFER_H__
