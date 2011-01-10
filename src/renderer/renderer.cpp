#include "pch.h"
#include "renderer.h"
#include "glew.h"

namespace Render {

bool init()
{
	GLenum err = glewInit();
	if(err != GLEW_OK)
		return false;

	return true;
}

void close()
{
}

}	// Render
