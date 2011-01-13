#include "pch.h"
#include "loader.h"
#include "../resource.h"

namespace Loader {

void registerLoaders(ResourceManager* mgr)
{
	mgr->addFactory(createBmp, loadBmp);
	mgr->addFactory(createPng, loadPng);
	mgr->addFactory(createJpeg, loadJpeg);
}

}	// namespace Loader
