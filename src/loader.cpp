#include "pch.h"
#include "loader.h"
#include "resource.h"

namespace Loader {

void registerLoaders(ResourceManager* mgr)
{
#ifdef RHINOCA_IOS
	mgr->addFactory(createImage, loadImage);
#else
	mgr->addFactory(createBmp, loadBmp);
	mgr->addFactory(createPng, loadPng);
	mgr->addFactory(createJpeg, loadJpeg);
#endif
	mgr->addFactory(createOgg, loadOgg);
	mgr->addFactory(createWave, loadWave);
}

}	// namespace Loader