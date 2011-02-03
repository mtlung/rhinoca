#ifndef __RENDERER_GLEW_H__
#define __RENDERER_GLEW_H__

#include "../rhinoca.h"

#ifdef RHINOCA_IOS
#	include <OpenGLES/ES1/gl.h>
#	include <OpenGLES/ES1/glext.h>
#	include <OpenGLES/ES2/gl.h>
#	include <OpenGLES/ES2/glext.h>
#else
#	define GLEW_STATIC
#	include "glew/glew.h"
#endif

#endif	// __RENDERER_GLEW_H__
