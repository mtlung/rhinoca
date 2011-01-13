#include "pch.h"
#include "../render/texture.h"

namespace Loader {

Resource* createBmp(const char* uri, ResourceManager* mgr)
{
	return new Render::Texture(uri);
}

bool loadBmp(Resource* resource, ResourceManager* mgr)
{
	return false;
}

}	// namespace Loader
