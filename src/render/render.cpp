#include "pch.h"
#include "render.h"
#include "gl.h"

namespace Render {

bool init()
{
#ifdef RHINOCA_WINDOWS
	GLenum err = glewInit();
	if(err != GLEW_OK)
		return false;
#endif

	return true;
}

void close()
{
}

}	// Render
